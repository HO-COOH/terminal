/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Row.hpp

Abstract:
- data structure for information associated with one row of screen buffer

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
--*/

#pragma once

#include "til/rle.h"
#include "LineRendition.hpp"
#include "OutputCell.hpp"
#include "OutputCellIterator.hpp"

#pragma warning(push, 1)

class TextBuffer;

struct InassignableStringView : public std::wstring_view
{
    using std::wstring_view::wstring_view;

    constexpr basic_string_view& operator=(const basic_string_view&) noexcept = delete;
    constexpr basic_string_view& operator=(basic_string_view&&) noexcept = delete;
};

struct InassignableDbcsAttribute : public DbcsAttribute
{
    using DbcsAttribute::DbcsAttribute;

    constexpr DbcsAttribute& operator=(const DbcsAttribute&) noexcept = delete;
    constexpr DbcsAttribute& operator=(DbcsAttribute&&) noexcept = delete;
};

enum class DelimiterClass
{
    ControlChar,
    DelimiterChar,
    RegularChar
};

struct RowTextIterator
{
    RowTextIterator(wchar_t* chars, uint16_t* indices, size_t cols, size_t beg, size_t end) noexcept :
        _chars{ chars },
        _indices{ indices },
        _cols{ cols },
        _beg{ beg },
        _end{ end }
    {
        operator++();
    }

    bool operator==(const RowTextIterator& other) const noexcept
    {
        return _beg == other._beg;
    }

    RowTextIterator& operator++()
    {
        _beg = _end;

        const auto current = _indices[_end];
        while (_end < _cols && _indices[++_end] == current)
        {
        }

        return *this;
    }

    const RowTextIterator& operator*() const noexcept
    {
        return *this;
    }

    std::wstring_view Text() const noexcept
    {
        return { _chars + _indices[_beg], gsl::narrow_cast<size_t>(_indices[_end] - _indices[_beg]) };
    }

    til::CoordType Cols() const noexcept
    {
        return _end - _beg;
    }

    InassignableDbcsAttribute DbcsAttr() const noexcept
    {
        return Cols() == 2 ? DbcsAttribute::Attribute::Leading : DbcsAttribute::Attribute::Single;
    }

private:
    const wchar_t* _chars;
    const uint16_t* _indices;
    size_t _cols;
    size_t _beg;
    size_t _end;
};

class ROW final
{
public:
    ROW(wchar_t* buffer, uint16_t* indices, uint16_t rowWidth, const TextAttribute& fillAttribute);

    ~ROW()
    {
        if (_charsCapacity != _indicesCount)
        {
            delete[] _chars;
        }
        if (_dbcsPaddedColumns)
        {
            delete[] _dbcsPaddedColumns;
        }
    }

    void SetWrapForced(const bool wrap) noexcept { _wrapForced = wrap; }
    bool WasWrapForced() const noexcept { return _wrapForced; }

    void SetDoubleBytePadded(const bool doubleBytePadded) noexcept { _doubleBytePadded = doubleBytePadded; }
    bool WasDoubleBytePadded() const noexcept { return _doubleBytePadded; }

    LineRendition GetLineRendition() const noexcept { return _lineRendition; }
    void SetLineRendition(const LineRendition lineRendition) noexcept { _lineRendition = lineRendition; }

    bool Reset(const TextAttribute& Attr);

    void ClearColumn(const size_t column);

    OutputCellIterator WriteCells(OutputCellIterator it, const til::CoordType index, const std::optional<bool> wrap = std::nullopt, std::optional<til::CoordType> limitRight = std::nullopt);

    void Resize(wchar_t* chars, uint16_t* indices, const size_t newWidth);

    const til::small_rle<TextAttribute, uint16_t, 1>& Attributes() const noexcept
    {
        return _attr;
    }

    void TransferAttributes(const til::small_rle<TextAttribute, uint16_t, 1>& attr, uint16_t newWidth)
    {
        _attr = attr;
        _attr.resize_trailing_extent(newWidth);
    }

    TextAttribute GetAttrByColumn(const uint16_t column) const
    {
        return _attr.at(column);
    }

    std::vector<uint16_t> GetHyperlinks() const
    {
        std::vector<uint16_t> ids;
        for (const auto& run : _attr.runs())
        {
            if (run.value.IsHyperlink())
            {
                ids.emplace_back(run.value.GetHyperlinkId());
            }
        }
        return ids;
    }

    bool SetAttrToEnd(const uint16_t beginIndex, const TextAttribute attr)
    {
        _attr.replace(gsl::narrow<uint16_t>(beginIndex), _attr.size(), attr);
        return true;
    }

    void ReplaceAttrs(const TextAttribute& toBeReplacedAttr, const TextAttribute& replaceWith)
    {
        _attr.replace_values(toBeReplacedAttr, replaceWith);
    }

    void Replace(const uint16_t beginIndex, const uint16_t endIndex, const TextAttribute& newAttr)
    {
        _attr.replace(beginIndex, endIndex, newAttr);
    }

    void ReplaceCharacters(size_t x, size_t width, const std::wstring_view& chars);

    size_t size() const noexcept
    {
        return _indicesCount;
    }

    size_t MeasureLeft() const noexcept
    {
        const auto beg = _chars;
        const auto end = beg + _indices[_indicesCount];
        auto it = beg;

        for (; it != end; ++it)
        {
            if (*it != L' ')
            {
                break;
            }
        }

        return static_cast<size_t>(it - beg);
    }

    size_t MeasureRight() const
    {
        const auto beg = _chars;
        const auto end = beg + _indices[_indicesCount];
        // We can always subtract 1, because _indicesCount/_charsCount are always greater 0.
        auto it = end;

        for (; it != beg; --it)
        {
            if (it[-1] != L' ')
            {
                break;
            }
        }

        return static_cast<size_t>(it - beg);
    }

    void ClearCell(const size_t column);

    bool ContainsText() const noexcept
    {
        auto it = _chars;
        const auto end = it + _indices[_indicesCount];

        for (; it != end; ++it)
        {
            if (*it != L' ')
            {
                return true;
            }
        }

        return false;
    }

    InassignableStringView GlyphAt(size_t column) const noexcept
    {
        column = std::min(column, _indicesCount - 1);

        const auto current = _indices[column];
        while (column <= _indicesCount && _indices[++column] == current)
        {
        }

        const auto len = gsl::narrow_cast<size_t>(_indices[column] - current);
        return { _chars + current, len };
    }

    InassignableDbcsAttribute DbcsAttrAt(size_t column) const noexcept
    {
        column = std::min(column, _indicesCount - 1);

        const auto idx = _indices[column];

        auto attr = DbcsAttribute::Attribute::Single;
        if (column > 0 && _indices[column - 1] == idx)
        {
            attr = DbcsAttribute::Attribute::Trailing;
        }
        else if (column < _indicesCount && _indices[column + 1] == idx)
        {
            attr = DbcsAttribute::Attribute::Leading;
        }

        return { attr };
    }

    InassignableStringView GetText() const
    {
        return { _chars, _indices[_indicesCount] };
    }

    const DelimiterClass DelimiterClassAt(size_t column, const std::wstring_view& wordDelimiters) const noexcept
    {
        column = std::min(column, _indicesCount - 1);

        const auto glyph = _chars[_indices[column]];

        if (glyph <= L' ')
        {
            return DelimiterClass::ControlChar;
        }
        else if (wordDelimiters.find(glyph) != std::wstring_view::npos)
        {
            return DelimiterClass::DelimiterChar;
        }
        else
        {
            return DelimiterClass::RegularChar;
        }
    }

    RowTextIterator CharsBegin() const noexcept
    {
        return { _chars, _indices, _indicesCount, 0, 0 };
    }

    RowTextIterator CharsEnd() const noexcept
    {
        return { _chars, _indices, _indicesCount, _indicesCount, _indicesCount };
    }

    auto AttrBegin() const noexcept
    {
        return _attr.begin();
    }

    auto AttrEnd() const noexcept
    {
        return _attr.end();
    }

#ifdef UNIT_TESTING
    friend constexpr bool operator==(const ROW& a, const ROW& b) noexcept;
    friend class RowTests;
#endif

private:
    void _resizeChars(size_t ch0, size_t ch1, size_t newCh1, size_t col3);

    [[nodiscard]] bool* _getDbcsPaddedColumns() noexcept
    {
        if (!_dbcsPaddedColumns)
        {
            _dbcsPaddedColumns = new bool[_indicesCount]{};
        }
        return _dbcsPaddedColumns;
    }

    wchar_t* _chars = nullptr;
    size_t _charsCapacity = 0;
    uint16_t* _indices = nullptr;
    size_t _indicesCount = 0;
    bool* _dbcsPaddedColumns = nullptr;

    til::small_rle<TextAttribute, uint16_t, 1> _attr;

    LineRendition _lineRendition = LineRendition::SingleWidth;
    // Occurs when the user runs out of text in a given row and we're forced to wrap the cursor to the next line
    bool _wrapForced = false;
    // Occurs when the user runs out of text to support a double byte character and we're forced to the next line
    bool _doubleBytePadded = false;
};

#ifdef UNIT_TESTING
constexpr bool operator==(const ROW& a, const ROW& b) noexcept
{
    // comparison is only used in the tests; this should suffice.
    return a._chars == b._chars;
}
#endif

#pragma warning(pop)
