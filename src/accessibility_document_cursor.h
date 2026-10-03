#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_DOCUMENT_CURSOR_H
#define CATA_SRC_ACCESSIBILITY_DOCUMENT_CURSOR_H

#include <cstddef>
#include <vector>

namespace accessibility
{

/** Retain independent reading positions when switching between documents. */
class document_cursor_memory
{
    public:
        explicit document_cursor_memory( size_t document_count );
        int position( size_t document ) const;
        int move( size_t document, int line_count, int delta );
        void reset();

    private:
        std::vector<int> positions_;
};

} // namespace accessibility

#endif // CATA_SRC_ACCESSIBILITY_DOCUMENT_CURSOR_H
