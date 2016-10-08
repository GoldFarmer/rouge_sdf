#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/filesystem.hpp>


enum FileOrigin
{
    FileOriginBegin,
    FileOriginCurrent,
    FileOriginEnd
};


class BlockBase
{
public:
    BlockBase() : references(0) {}
    virtual ~BlockBase() {}
    //virtual get method
    virtual void Read(void *data, size_t offset, size_t size) = 0;
    //template get methods
    template <typename T>
    T Get(size_t offset)
    {
        T result{};
        Get(&result, offset, 1);
        return result;
    }
    template <typename T>
    std::unique_ptr<T[]> Get(size_t offset, size_t count)
    {
        auto size = count*sizeof(T);
        std::unique_ptr<T[]> result = std::make_unique<T[]>(size);
        Get(result.get(), offset, count);
        return std::move(result);
    }
    template <typename T>
    void Get(T *result, size_t offset, size_t count)
    {
        static_assert(std::is_trivial<T>::value, "Only trivial objects can be read");
        Read(static_cast<void*>(result), offset, count*sizeof(T));
    }
    //size of block
    virtual size_t Size() = 0;
    size_t references;
};


//typedef std::shared_ptr<BlockBase> BlockPtr;
template <typename X>
inline void intrusive_ptr_add_ref(X* x) {
    ++x->references;
}
template <typename X>
inline void intrusive_ptr_release(X* x) {
    if (--x->references == 0)
        delete x;
}


typedef boost::intrusive_ptr<BlockBase> BlockPtr;



class BlockDisk : public BlockBase
{
public:
    BlockDisk(const wchar_t *fileName)
        : fileName_(fileName)
    {
        Open();
        file.seekg(0, std::ios_base::end);
        fileSize = size_t(file.tellg());
        file.seekg(0, std::ios_base::beg);
        Close();
    }
    virtual void Read(void *data, size_t offset, size_t size) override
    {
        if (offset + size > fileSize)
            throw std::runtime_error("Going beyond file");

        Open();
        file.seekg(offset);
        file.read((char*)data, size);
        if (!file.good())
        {
            Close();
            throw std::runtime_error("File read error");
        }
        Close();
    }
    virtual size_t Size() override
    {
        return fileSize;
    }
private:
    void Open()
    {
        if (!file.good())
        {
            throw std::runtime_error("File open error");
        }
        file.open(fileName_, std::ios::binary);
    }
    void Close()
    {
        file.close();
    }
    std::wstring fileName_;
    std::ifstream  file;
    size_t fileSize;
};

class BlockMemory : public BlockBase
{
public:
    BlockMemory(const BlockPtr &file)
        :blockSize(file->Size())
    {
        blockData = std::make_unique<unsigned char[]>(blockSize);
        file->Get(blockData.get(), 0, blockSize);
    }
    BlockMemory(std::unique_ptr<unsigned char[]>  &&data, size_t size)
        :blockSize(size), blockData(std::move(data))
    {
    }
    BlockMemory(const void *data, size_t size)
        :blockSize(size)
    {
        blockData = std::make_unique<unsigned char[]>(size);
        std::memcpy(blockData.get(), data, size);
    }
    virtual void Read(void *data, size_t offset, size_t size) override
    {
        if (offset + size > blockSize)
            throw std::runtime_error("Memory file index out of range");
        std::memcpy(data, blockData.get() + offset, size);
    }
    virtual size_t Size() override
    {
        return blockSize;
    }
private:
    std::unique_ptr<unsigned char[]> blockData;
    size_t blockSize;
};

class BlockPart : public BlockBase
{
public:
    BlockPart(BlockPtr file, size_t offset, size_t size)
        : size_(size), offset_(offset), file_(file)
    {
        if (offset_ + size_ > file->Size())
            throw std::exception("File create error: part file");
        BlockPart *sourceFile = dynamic_cast<BlockPart*>(file.get());
        if (sourceFile)
        {
            file_ = sourceFile->file_;
            offset_ += sourceFile->offset_;
        }
    }
    virtual void Read(void *data, size_t offset, size_t size) override
    {
        if (offset + size > size_)
            throw std::exception("File read error: part file");
        return file_->Read(data, offset + offset_, size);
    }
    virtual size_t Size() override
    {
        return size_;
    }
private:
    BlockPtr file_;
    size_t offset_;
    size_t size_;
};

BlockPtr MakeBlockPart(BlockPtr base, size_t offset, size_t size)
{
    return BlockPtr(new BlockPart(base, offset, size));
}
BlockPtr MakeBlockPair(BlockPtr block1, BlockPtr block2)
{
    size_t size = block1->Size() + block2->Size();
    std::unique_ptr<unsigned char[]> data = std::make_unique<unsigned char[]>(size);
    block1->Get(data.get(), 0, block1->Size());
    block2->Get(data.get() + block1->Size(), 0, block2->Size());
    return BlockPtr(new BlockMemory(std::move(data), size));
}
template <typename T>
BlockPtr MakeBlockMemory(const std::vector<T> &data)
{
    return BlockPtr(new BlockMemory(data.data(), data.size()*sizeof(T)));
}
BlockPtr MakeBlockMemory(BlockPtr base)
{
    return BlockPtr(new BlockMemory(base));
}
BlockPtr MakeBlockMemory(const void *data, size_t size)
{
    return BlockPtr(new BlockMemory(data, size));
}
BlockPtr MakeBlockMemory(std::unique_ptr<unsigned char[]>  &&data, size_t size)
{
    return BlockPtr(new BlockMemory(std::move(data), size));
}
BlockPtr MakeBlockDisk(const wchar_t *filePath)
{
    return BlockPtr(new BlockDisk(filePath));
}
BlockPtr MakeBlockDisk(const std::wstring &filePath)
{
    return MakeBlockDisk(filePath.c_str());
}

template <typename T>
class DataArray
{
public:

    class Iterator : public boost::iterator_facade<Iterator, const T, boost::random_access_traversal_tag>
    {
    public:
        Iterator(const T* iter, size_t size)
            : iterBegin(iter)
            , iter(iter)
            , iterEnd(iter + size)
        {}
        void increment()
        {
            iter++;
        }
        void decrement()
        {
            iter--;
        }
        void advance(std::ptrdiff_t n)
        {
            iter += n;
        }
        std::ptrdiff_t distance_to(const Iterator &z) const
        {
            return iter - z.iter;
        }
        const T &dereference() const
        {
            if (iter < iterBegin || iter >= iterEnd)
                throw std::exception("Index out of range");
            return *iter;
        }
        bool equal(const Iterator &z) const
        {
            return iter == z.iter;
        }
    private:
        const T* iterBegin;
        const T* iter;
        const T* iterEnd;
    };
    DataArray()
        : offset(0)
        , count(0)
    {
    }
    DataArray(const BlockPtr &block, size_t offset, size_t count)
        : offset(offset)
        , count(count)
        , data(std::move(block->Get<T>(offset, count)))
    {
    }
    size_t Size()
    {
        return count;
    }
    T operator[](size_t index) const
    {
        //ERROR_STACK(index);
        if (index >= count)
            throw std::exception("Array index out of range");
        return data[index];
    }
    Iterator begin() const
    {
        return Iterator(data.get(), count);
    }
    Iterator end() const
    {
        return Iterator(data.get(), count) + count;
    }
    operator bool() const
    {
        return count != 0;
    }
private:
    std::unique_ptr<T[]> data;
    size_t offset;
    size_t count;
};



class File : public BlockBase
{
public:
    File(BlockPtr block)
        : block(block)
        , position(0)
    {

    }
    virtual size_t Size() override
    {
        return block->Size();
    }
    inline size_t Tell()
    {
        return position;
    }
    inline bool Eof()
    {
        return position == Size();
    }
    inline void Seek(size_t newPosition, FileOrigin origin = FileOriginBegin)
    {
        switch (origin)
        {
        case FileOriginBegin:
        {
            if (newPosition > Size())
                throw std::exception("File seek error: virtual file");
            position = newPosition;
        }
        break;
        case FileOriginCurrent:
        {
            if (newPosition + position> Size())
                throw std::exception("File seek error: virtual file");
            position += newPosition;
        }
        break;
        case FileOriginEnd:
        {
            if (newPosition > Size())
                throw std::exception("File seek error: virtual file");
            position = Size() - newPosition;
        }
        break;
        }
    }
    virtual void Read(void *data, size_t offset, size_t size) override
    {
        return block->Read(data, offset, size);
    }
    template <typename T>
    inline T Read()
    {
        size_t readPosition = position;
        position += sizeof(T);
        return BlockBase::Get<T>(readPosition);
    }
    template <typename T>
    inline const void Read(T *elements, size_t elementCount)
    {
        size_t readPosition = position;
        position += sizeof(T)*elementCount;
        BlockBase::Get<T>(elements, readPosition, elementCount);
    }
    inline BlockPtr Part(size_t offset, size_t size)
    {
        return MakeBlockPart(block, offset, size);
    }
    inline BlockPtr Part(size_t size)
    {
        size_t oldPos = position;
        if (size + position> Size())
            throw std::exception("File part error: virtual file");
        position += size;
        return MakeBlockPart(block, oldPos, size);
    }
    inline BlockPtr Part()
    {
        return Part(block->Size() - position);
    }
    template <typename T>
    DataArray<T> Array(size_t elementCount)
    {
        size_t oldPosition = position;
        if (sizeof(T)*elementCount + position> Size())
            throw std::exception("File part error: virtual file");
        position += sizeof(T)*elementCount;
        return ReadArray<T>(block, oldPosition, elementCount);
    }
    void Align(size_t aligment)
    {
        size_t mod = position % aligment;
        if (mod)
        {
            position += aligment - mod;
        }
    }
private:
    size_t position;
    BlockPtr block;
};


File MakeFileDisk(const wchar_t *filePath)
{
    return File(MakeBlockDisk(filePath));
}
File MakeFileDisk(const std::wstring &filePath)
{
    return MakeFileDisk(filePath.c_str());
}

void WriteBlock(BlockPtr block, const wchar_t *filePath)
{
    std::ofstream file(filePath, std::ios::binary);
    file.write(block->Get<char>(0, block->Size()).get(), block->Size());
}
void WriteBlock(BlockPtr block, const std::wstring &filePath)
{
    WriteBlock(block, filePath.c_str());
}
void WriteBlockApp(BlockPtr block, const wchar_t *filePath)
{
    std::ofstream file(filePath, std::ios::binary | std::ios::app | std::ios::ate);
    file.write(block->Get<char>(0, block->Size()).get(), block->Size());
}
void WriteBlockApp(BlockPtr block, const std::wstring &filePath)
{
    WriteBlockApp(block, filePath.c_str());
}

template <typename T>
DataArray<T> ReadArray(BlockPtr block, size_t offset, size_t count)
{
    return DataArray<T>(block, offset, count);
}

