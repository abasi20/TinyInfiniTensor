#include "core/allocator.h"
#include <utility>

namespace infini
{
    Allocator::Allocator(Runtime runtime) : runtime(runtime)
    {
        used = 0;
        peak = 0;
        ptr = nullptr;

        // 'alignment' defaults to sizeof(uint64_t), because it is the length of
        // the longest data type currently supported by the DataType field of
        // the tensor
        alignment = sizeof(uint64_t);
    }

    Allocator::~Allocator()
    {
        if (this->ptr != nullptr)
        {
            runtime->dealloc(this->ptr);
        }
    }

    size_t Allocator::alloc(size_t size)
    {
        IT_ASSERT(size > 0);
        IT_ASSERT(this->ptr == nullptr);
        // pad the size to the multiple of alignment
        size = this->getAlignedSize(size);
        size_t ret_offset = 0;
        // =================================== 作业 ===================================
        // TODO: 设计一个算法来分配内存，返回起始地址偏移量
        // =================================== 作业 ===================================
        auto tar_free_block = free_blocks.end();//记录最小适配空闲位置
        auto end_free_block = tar_free_block;
        for(auto it = free_blocks.begin(); it!=free_blocks.end(); it++){
            if(it->second >= size){
                
                if(tar_free_block!= end_free_block){
                    if(it->second< tar_free_block->second){
                        tar_free_block = it;
                    }
                }else{
                    tar_free_block=it;
                }
            }
        }
        if(tar_free_block != end_free_block){//找到了合适的空间
            ret_offset = tar_free_block->first + tar_free_block->second - size;//从后向前alloc空间
            tar_free_block->second -=size;
            if(tar_free_block->second==0) free_blocks.erase(tar_free_block);
        }else{//没有找到
            ret_offset = used;
            used += size;
            peak = used; //peak 只需要在used变化是进行更新
        }
        
        return ret_offset;//
    }

    void Allocator::free(size_t addr, size_t size)
    {
        IT_ASSERT(this->ptr == nullptr);
        size = getAlignedSize(size);

        // =================================== 作业 ===================================
        // TODO: 设计一个算法来回收内存
        // =================================== 作业 ===================================
        free_blocks[addr] = size;//std::map 有序
        used -= size;
        auto last = free_blocks.begin();
        if(last == free_blocks.end()) return;
        auto it = last;
        it++;
        for(;it!=free_blocks.end();){
            if(last->first+last->second == it->first){
                auto need_remove = it;
                it++;
                free_blocks.erase(need_remove);
            }else{
                last = it;
                it++;
            }
        }
    
    }

    void *Allocator::getPtr()
    {
        if (this->ptr == nullptr)
        {
            this->ptr = runtime->alloc(this->peak);
            printf("Allocator really alloc: %p %lu bytes\n", this->ptr, peak);
        }
        return this->ptr;
    }

    size_t Allocator::getAlignedSize(size_t size)
    {
        return ((size - 1) / this->alignment + 1) * this->alignment;
    }

    void Allocator::info()
    {
        std::cout << "Used memory: " << this->used
                  << ", peak memory: " << this->peak << std::endl;
    }
}
