// Copyright (C) 2015 Kristopher Johnson
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef net_kristopherjohnson_HexDump_h
#define net_kristopherjohnson_HexDump_h

#include <ios>
#include <iomanip>
#include <span>
#include <sstream>
#include <vector>

namespace Support::HexDump {

    using std::endl;
    using std::hex;
    using std::isprint;
    using std::left;
    using std::ostringstream;
    using std::setfill;
    using std::setw;
    using std::size_t;
    using std::vector;

    // In the templates below, the type parameters must satisfy these constraints:
    //
    // - Stream: something like std::ostream or std::istream
    //
    // - InputStream: something like std::istream
    //
    // - OutputStream: something like std::ostream
    //
    // - ByteSequence: a collection of chars that can be iterated over, like std::vector<char>

    const size_t BytesPerLine = 16;

    // Saves original formatting state for a stream and
    // restores that state before going out of scope
    template<typename Stream>
    class SavedState
    {
    public:
        SavedState(Stream& s)
        : stream(s), oldFlags(s.flags()), oldFill(s.fill())
        {}

        ~SavedState() { stream.flags(oldFlags); stream.fill(oldFill); }

        SavedState(const SavedState&) = delete;
        void operator=(const SavedState&) = delete;

    private:
        Stream& stream;
        decltype(stream.flags()) oldFlags;
        decltype(stream.fill()) oldFill;
    };

    // Dump a sequence of bytes as hex with spaces between; e.g., "cf fa 4f a0 "
    template<typename OutputStream, typename ByteSequence>
    void dumpBytesAsHex(OutputStream& output, const ByteSequence& bytes)
    {
        SavedState<OutputStream> savedState{ output };

        output << hex << setfill('0');

        for (auto byte: bytes) {
            unsigned widenedUIntValue = static_cast<unsigned char>(byte);
            output << setw(2) << widenedUIntValue << " ";
        }
    }

    // Dump a sequence of bytes as ASCII characters,
    // substituting '.' for non-printing characters
    template<typename OutputStream, typename ByteSequence>
    void dumpBytesAsText(OutputStream& output, const ByteSequence& bytes)
    {
        for (auto byte: bytes)
            output.put(isprint(byte) ? byte : '.');
    }

    // Dump a sequence of bytes in side-by-side hex and text formats
    template<typename OutputStream, typename ByteSequence>
    void dumpHexLine(OutputStream& output, const ByteSequence& bytes)
    {
        SavedState<OutputStream> savedState{ output };

        ostringstream hexStream;
        dumpBytesAsHex(hexStream, bytes);
        const auto HexOutputWidth = BytesPerLine * 3 + 1;
        output << setw(HexOutputWidth) << left << hexStream.str();

        dumpBytesAsText(output, bytes);

        output << endl;
    }

    // Dump a sequence of bytes in side-by-side hex and text formats,
    // prefixed with a hex offset
    template<typename OutputStream, typename ByteSequence>
    void dumpHexLine(OutputStream& output, size_t offset, const ByteSequence& bytes)
    {
        {
            SavedState<OutputStream> savedState{ output };
            output << setw(8) << setfill('0') << hex
                   << offset << "  ";
        }

        dumpHexLine(output, bytes);
    }

    // Dump bytes from input stream in side-by-side hex and text formats
    template<typename OutputStream, typename InputStream>
    void dumpStream(OutputStream& output, InputStream& input)
    {
        vector<char> bytesToDump;
        bytesToDump.reserve(BytesPerLine);

        size_t offset = 0;

        char byte;
        while (input.get(byte)) {
            bytesToDump.push_back(byte);

            if (bytesToDump.size() == BytesPerLine) {
                dumpHexLine(output, offset, bytesToDump);
                bytesToDump.clear();
                offset += BytesPerLine;
            }
        }

        if (!bytesToDump.empty())
            dumpHexLine(output, offset, bytesToDump);
    }

    // Dump bytes from buffer in side-by-side hex and text formats
    template<typename OutputStream, typename ByteType>
    void dumpBuffer(OutputStream& output, std::span<const ByteType> input)
    {
        vector<char> bytesToDump;
        bytesToDump.reserve(BytesPerLine);

        size_t offset{0}, bytesRead{0};

        while(bytesRead < input.size()) {
            bytesToDump.push_back(static_cast<char>(input[bytesRead++]));

            if(bytesToDump.size() == BytesPerLine) {
                dumpHexLine(output, offset, bytesToDump);
                bytesToDump.clear();
                offset += BytesPerLine;
            }
        }

        if(!bytesToDump.empty()) {
            dumpHexLine(output, offset, bytesToDump);
        }
    }

} // namespace HexDump

#endif /* net_kristopherjohnson_HexDump_h */
