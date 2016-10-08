#include "BasicFile.hpp"
#include "utils.h"
#include <zlib.h>
#include <boost\filesystem.hpp>
#include <boost\format.hpp>

#pragma pack(push,1)
struct SdfTocHeader
{
    uint32_t fileTag; //0x54534557
    uint32_t fileVersion;
    uint32_t decompressedSize;
    uint32_t compressedSize;
    uint32_t zero;
    uint32_t block1count;
    uint32_t ddsHeaderBlockCount;
};
struct SdfTocId
{
    uint64_t ubisoft;
    uint8_t data[0x20];
    uint64_t massive;
};
struct SdfDdsHeader
{
    uint32_t usedBytes;
    uint8_t bytes[0x94];
};

#pragma pack(pop)



struct FileTree
{
    template <typename Callback>
    static void ParseNames(File data, const Callback &cb, std::string name="")
    {

        auto readVariadicInteger = [&data](uint32_t count)
        {
            uint64_t result = 0;

            for (uint32_t i = 0; i < count; i++)
            {
                result |= uint64_t(data.Read<uint8_t>()) << (i * 8);
            }
            return result;
        };
        auto ch = data.Read<char>();
        if (ch == 0)
            throw std::exception("Unexcepted byte in file tree");
        if (ch >= 1 && ch <= 0x1f) //string part
        {
            while (ch--)
            {
                name += data.Read<char>();
            }
            ParseNames(data, cb, name);
        }
        else if (ch >= 'A' && ch <= 'Z') //file entry
        {

            ch = ch - 'A';
            auto count1 = ch & 7;
            auto flag1 = (ch >> 3) & 1;

            if (count1)
            {
                uint32_t strangeId = data.Read<uint32_t>();
                auto ch2 = data.Read<uint8_t>();
                auto byteCount = ch2 & 3;
                auto byteValue = ch2 >> 2;
                uint64_t ddsType = readVariadicInteger(byteCount);

                for (int chunkIndex = 0;chunkIndex<count1;chunkIndex++)
                {
                    auto ch3 = data.Read<uint8_t>();
                    auto compressedSizeByteCount = (ch3 & 3) + 1;
                    auto packageOffsetByteCount = (ch3 >> 2) & 7;
                    auto hasCompression = (ch3 >> 5) & 1;

                    uint64_t decompressedSize = readVariadicInteger(compressedSizeByteCount);
                    uint64_t compressedSize = 0;
                    uint64_t packageOffset = 0;
                    if (hasCompression)
                    {
                        compressedSize = readVariadicInteger(compressedSizeByteCount);
                    }
                    if (packageOffsetByteCount)
                    {
                        packageOffset = readVariadicInteger(packageOffsetByteCount);
                    }
                    uint64_t packageId = readVariadicInteger(2);

                    std::vector<uint64_t> compSizeArray;

                    if (hasCompression)
                    {
                        uint64_t pageCount = (decompressedSize + 0xffff) >> 16;
                        if (pageCount > 1)
                        {
                            for (uint64_t page = 0; page < pageCount; page++)
                            {
                                uint64_t compSize = readVariadicInteger(2);
                                compSizeArray.push_back(compSize);
                            }
                        }
                    }

                    uint64_t fileId = readVariadicInteger(4);

                    if (compSizeArray.size() == 0 && hasCompression)
                        compSizeArray.push_back(compressedSize);

                    cb(name, packageId, packageOffset, decompressedSize, compSizeArray, ddsType, chunkIndex != 0, byteCount != 0 && chunkIndex == 0);

                }

            }
            if (flag1)
            {
                auto ch3 = data.Read<uint8_t>();
                while (ch3--)
                {
                    auto ch3_1 = data.Read<uint8_t>();
                    auto ch3_2 = data.Read<uint8_t>();
                }
            }
        }
        else //search tree entry
        {
            File data2 = data;
            uint32_t offset = data.Read<uint32_t>();
            data2.Seek(offset);
            ParseNames(data, cb, name);
            ParseNames(data2, cb, name);
        }
    }
};


int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3)
    {
        std::cout << "Tom Clancy's The Division .sdftoc extractor v2" << std::endl;
        std::cout << "usage: rouge_sdf.exe <.sdftoc path> <output directory>" << std::endl;
        return 0;
    }
    try
    {

        std::wstring sdfTocFile = argv[1];
        std::wstring outputDir = argv[2];

        outputDir = boost::filesystem::path(outputDir).remove_trailing_separator().wstring() + L"\\";

        auto file = MakeFileDisk(sdfTocFile);


        SdfTocHeader header = file.Read<SdfTocHeader>();
        SdfTocId id = file.Read<SdfTocId>();
        uint8_t signExistFlag = file.Read<uint8_t>();
        if (signExistFlag)
        {
            file.Seek(0x140, FileOriginCurrent);
        }

        auto block1 = file.Array<uint32_t>(header.block1count);;
        auto block11 = file.Array<SdfTocId>(header.block1count);;
        auto ddsHeaderBlock = file.Array<SdfDdsHeader>(header.ddsHeaderBlockCount);


        std::unique_ptr<uint8_t[]> decompressed = std::make_unique<uint8_t[]>(header.decompressedSize);
        std::unique_ptr<uint8_t[]> compressed = std::make_unique<uint8_t[]>(header.compressedSize);

        file.Read<uint8_t>(compressed.get(), header.compressedSize);
        uLong decompSize = header.decompressedSize;
        uncompress(decompressed.get(), &decompSize, compressed.get(), header.compressedSize);
        File f = File(MakeBlockMemory(std::move(decompressed), decompSize));
        FileTree::ParseNames(f, [&](const std::string &name, uint64_t packageId, uint64_t packageOffset,
            uint64_t decompressedSize, const std::vector<uint64_t> & compSizeArray,
            uint64_t ddsType, bool append, bool useDDS)
        {


            std::cout << name << std::endl;

            boost::filesystem::path sdfTocPath(sdfTocFile);
            std::wstring layer;
            if (packageId < 1000)
            {
                layer = L"A";
            }
            else if (packageId < 2000)
            {
                layer = L"B";
            }
            else
            {
                layer = L"C";
            }
            typedef  boost::basic_format<wchar_t >  wformat;
            std::wstring dataFormated = boost::str(boost::wformat(L"-%s-%04i.sdfdata") % layer % packageId);
            std::wstring sdfDataPath = sdfTocPath.parent_path().append(sdfTocPath.stem().wstring()).wstring() + dataFormated;

            if (!IsFileExist(sdfDataPath))
                return;

            BlockPtr fileBlock = MakeBlockDisk(sdfDataPath);

            std::wstring outFileName = outputDir + AnsiToUnicode(name);
            std::replace(outFileName.begin(), outFileName.end(), L'/', L'\\');
            CreateDirectoryRecursively(ExtractFilePath(outFileName));

            BlockPtr resultBlock;


            if (compSizeArray.size() == 0)
            {
                //decompressed
                resultBlock = MakeBlockPart(fileBlock, packageOffset, decompressedSize);
            }
            else
            {
                std::unique_ptr<uint8_t[]> decompressed = std::make_unique<uint8_t[]>(decompressedSize);
                uint64_t decompOffset = 0;
                uint64_t compOffset = 0;
                for (uint64_t compSizePart : compSizeArray)
                {
                    const uint64_t pageSize = 0x10000ull;
                    uLong decompSizePart = std::min(decompressedSize - decompOffset, pageSize);

                    if (compSizePart == 0 || decompSizePart == compSizePart)
                    {
                        fileBlock->Get<uint8_t>(decompressed.get() + decompOffset, packageOffset + compOffset, decompSizePart);
                        compSizePart = decompSizePart;
                    }
                    else
                    {
                        auto compressed = fileBlock->Get<uint8_t>(packageOffset + compOffset, compSizePart);
                        if (uncompress(decompressed.get() + decompOffset, &decompSizePart, compressed.get(), compSizePart) != Z_OK)
                            throw std::exception("Uncompress error");

                    }
                    decompOffset += decompSizePart;
                    compOffset += compSizePart;

                }
                resultBlock = MakeBlockMemory(std::move(decompressed), decompressedSize);
            }

            if (useDDS)
            {
                SdfDdsHeader ddsHeader = ddsHeaderBlock[ddsType];
                const int ddsHeaderDataSize = ddsHeader.usedBytes;
                auto fullDataBlockSize = ddsHeaderDataSize + resultBlock->Size();
                auto fullDataBlock = std::make_unique<uint8_t[]>(fullDataBlockSize);
                std::memcpy(fullDataBlock.get(), ddsHeader.bytes, ddsHeaderDataSize);
                resultBlock->Get(fullDataBlock.get() + ddsHeaderDataSize, 0, resultBlock->Size());
                resultBlock = MakeBlockMemory(std::move(fullDataBlock), fullDataBlockSize);
            }

            if (append)
            {
                WriteBlockApp(resultBlock, outFileName);
            }
            else
            {
                WriteBlock(resultBlock, outFileName);
            }

        });
    }
    catch (const std::exception & ex)
    {
        std::cout << "Error: " << ex.what() << std::endl;
    }
    return 0;
}