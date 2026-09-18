#include "accessibility.h"

#include <memory>
#include <string>

#if defined(_WIN32)
#include <array>
#include <cwchar>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "catacharset.h"
#endif

namespace accessibility
{
namespace
{

#if defined(_WIN32)

class nvda_backend final : public backend
{
    public:
        nvda_backend();
        ~nvda_backend() override;

        bool is_available() const override;
        bool announce( const std::string &message ) override;

    private:
        using test_if_running_fn = unsigned long( __stdcall * )();
        using speak_text_fn = unsigned long( __stdcall * )( const wchar_t * );

        bool is_default_desktop() const;

        HMODULE client = nullptr;
        test_if_running_fn test_if_running = nullptr;
        speak_text_fn speak_text = nullptr;
};

std::wstring controller_client_path()
{
    std::array<wchar_t, 32768> executable_path{};
    const DWORD length = GetModuleFileNameW( nullptr, executable_path.data(),
                         static_cast<DWORD>( executable_path.size() ) );
    if( length == 0 || length >= executable_path.size() ) {
        return {};
    }

    std::wstring path( executable_path.data(), length );
    const size_t separator = path.find_last_of( L"\\/" );
    if( separator == std::wstring::npos ) {
        return {};
    }

    path.resize( separator + 1 );
    path += L"nvdaControllerClient.dll";
    return path;
}

nvda_backend::nvda_backend()
{
    const std::wstring path = controller_client_path();
    if( path.empty() ) {
        return;
    }

    client = LoadLibraryExW( path.c_str(), nullptr,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32 );
    if( client == nullptr ) {
        return;
    }

    test_if_running = reinterpret_cast<test_if_running_fn>(
                          GetProcAddress( client, "nvdaController_testIfRunning" ) );
    speak_text = reinterpret_cast<speak_text_fn>(
                     GetProcAddress( client, "nvdaController_speakText" ) );
    if( test_if_running == nullptr || speak_text == nullptr ) {
        FreeLibrary( client );
        client = nullptr;
        test_if_running = nullptr;
        speak_text = nullptr;
    }
}

nvda_backend::~nvda_backend()
{
    if( client != nullptr ) {
        FreeLibrary( client );
    }
}

bool nvda_backend::is_default_desktop() const
{
    HDESK desktop = OpenInputDesktop( 0, FALSE, DESKTOP_READOBJECTS );
    if( desktop == nullptr ) {
        return false;
    }

    std::array<wchar_t, 256> desktop_name{};
    DWORD required_size = 0;
    const bool got_name = GetUserObjectInformationW( desktop, UOI_NAME, desktop_name.data(),
                          static_cast<DWORD>( desktop_name.size() * sizeof( wchar_t ) ),
                          &required_size ) != FALSE;
    CloseDesktop( desktop );

    return got_name && _wcsicmp( desktop_name.data(), L"Default" ) == 0;
}

bool nvda_backend::is_available() const
{
    return client != nullptr && is_default_desktop() && test_if_running() == 0;
}

bool nvda_backend::announce( const std::string &message )
{
    if( message.empty() || !is_available() ) {
        return false;
    }

    const std::wstring wide_message = utf8_to_wstr( message );
    return speak_text( wide_message.c_str() ) == 0;
}

#endif // _WIN32

} // namespace

std::unique_ptr<backend> make_platform_backend()
{
#if defined(_WIN32)
    return std::make_unique<nvda_backend>();
#else
    return nullptr;
#endif
}

} // namespace accessibility
