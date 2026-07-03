/**
 ******************************************************************************
 * Copyright(c) Infy Power 2026-2026
 * @file    pau_vector.c
 * @author  YBA40320
 * @version V1.0
 * @date    2026-04-27
 * @brief   仿照c++ stl的vector做一个存储4字节整型的矢量
 * @history 2026-04-27 YBA40320 创建;2026-05-15 YBA40320 从模拟机移植到A2605线环1500kW工程
 * @details
 *
 *************************************************************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pau_vector.h"
#include "pau_broker.h"
//  =============  PAU_Vector 核心函数 =============
//  初始化PAU_Vector（对应QVector<T>::QVector()）

//  init_capacity：初始容量
PAU_Vector *pau_vector_create(size_t init_capacity)
{
    if (init_capacity > PAU_VECTOR_DEFAULT_CAPACITY)
    {
        return NULL;
    }
    if (init_capacity == 0)
    {
        init_capacity = PAU_VECTOR_DEFAULT_CAPACITY;
    }

    PAU_Vector *vec = (PAU_Vector *)pau_calloc(sizeof(PAU_Vector) + (init_capacity + 1) * sizeof(size_t), __func__); // 申请内存4+1+1*init_capacity
    if (!vec)
    {
        return NULL;
    }
    vec->size = 0;
    vec->data[0] = init_capacity;
    for (int i = 1; i <= init_capacity; i++)
    {
        vec->data[i] = -1;
    }
    return vec;
}
// 复制PAU_Vector（对应QVector<T>::QVector(const QVector<T> &other)）
PAU_Vector *pau_vector_copy(PAU_Vector *vec)
{
    if (!vec)
        return NULL;

    PAU_Vector *new_vec = pau_vector_create(vec->size);
    if (!new_vec)
        return NULL;

    for (size_t i = 1; i <= vec->size; i++)
    {
        new_vec->data[i] = vec->data[i];
    }
    new_vec->size = vec->size;
    return new_vec;
}
// 获取元素个数(对应QVector::size())
size_t pau_vector_size(PAU_Vector *vec)
{
    return vec->size;
}

// 基础版：判断是否包含元素（字节对比）
int pau_vector_contains(PAU_Vector *vec, size_t target)
{
    if (!vec || !target || vec->size == 0)
        return -1;

    for (size_t i = 1; i <= vec->size; i++)
    {
        if (vec->data[i] == target)
        {
            return 1; // 找到目标元素
        }
    }
    return 0;
}
int pau_vector_contains_deep(PAU_Vector *vec, const size_t *target)
{
    if (!vec || !target || vec->size == 0)
        return -1;

    for (size_t i = 1; i <= vec->size; i++)
    {
        void *elem = (size_t *)vec->data + (1 + i);
        if (memcmp(elem, (void *)target, sizeof(size_t)) == 0)
        {
            return 1;
        }
    }
    return 0;
}
// 进阶版：支持自定义比较函数
int pau_vector_contains_ex(PAU_Vector *vec, const void *target, PAU_VectorCompareFunc cmp_func)
{
    if (!vec || !target || vec->size == 0)
        return -1;
    if (cmp_func == NULL)
    {
        cmp_func = (PAU_VectorCompareFunc)memcmp;
    }

    for (size_t i = 1; i <= vec->size; i++)
    {
        void *elem = (size_t *)vec->data + (1 + i);
        if (cmp_func(elem, target) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// 尾部追加元素（对应QVector::append()）
// elem：要添加的元素指针（如&num）

int pau_vector_append(PAU_Vector *vec, size_t elem)
{
    if (!vec)
        return -1;

    // 容量不足时
    if (vec->size == PAU_VECTOR_DEFAULT_CAPACITY)
    {
        return -1;
    }
    // 如果已经包含该元素
    if (1 == pau_vector_contains(vec, elem))
    {
        return -1;
    }

    // 计算元素存储位置，拷贝数据（泛型拷贝）
    size_t *target = (size_t *)vec->data + 1 + vec->size;
    *target = elem;
    vec->size++;
    return 0;
}
int pau_vector_append_deep(PAU_Vector *vec, const void *elem)
{
    if (!vec || !elem)
        return -1;

    // 容量不足时
    if (vec->size == PAU_VECTOR_DEFAULT_CAPACITY)
    {
        return -1;
    }
    // 如果已经包含该元素
    // if (1 == pau_vector_contains_deep(vec, *elem))
    // {
    //    return -1;
    // }
    // 计算元素存储位置，拷贝数据（泛型拷贝）
    size_t *target = (size_t *)vec->data + 1 + vec->size;
    memcpy(target, elem, sizeof(size_t));
    vec->size++;
    return 0;
}

// 按下标访问元素（对应QVector::operator[]）
// index：下标，返回元素指针（NULL表示越界）
size_t pau_vector_at(PAU_Vector *vec, size_t index)
{
    if (!vec || index > vec->size)
        return -1;
    return vec->data[index];
}

// 删除指定内容的元素（对应QVector::remove()）
int pau_vector_remove(PAU_Vector *vec, size_t elem)
{
    if (!vec)
        return -1;
    for (size_t i = 1; i <= vec->size; i++)
    {
        if (vec->data[i] == elem)
        {
            for (size_t j = i; j <= vec->size - 1; j++)
            {
                vec->data[j] = vec->data[j + 1];
            }
            vec->size--;
            return elem;
        }
    }
    return -1;
}

// 清空所有元素（保留容量，对应QVector::clear()）
void pau_vector_clear(PAU_Vector *vec)
{
    if (!vec)
    {
        return;
    }
    vec->size = 0;
    for (int i = 1; i <= vec->data[0] && i <= PAU_VECTOR_DEFAULT_CAPACITY; i++)
    {
        vec->data[i] = -1;
    }
}
// 填充所有元素（对应QVector::fill()）
void pau_vector_fill(PAU_Vector *vec, size_t value)
{
    if (!vec)
    {
        return;
    }
    for (int i = 1; i <= vec->size; i++)
        vec->data[i] = value;
}

// 销毁PAU_Vector（释放内存，对应QVector::~QVector()）
void pau_vector_destroy(PAU_Vector *vec)
{
    if (NULL != vec)
    {
        vec->data[0] = 0; // 销毁前修改容量为0 pau_alloc视为未使用
    }
}

void pau_vector_set(PAU_Vector *vec, size_t index, size_t value)
{
    if (!vec || index > vec->data[0])
        return;
    if (vec->data[index] != 0)
    {
        vec->size++;
    }
    vec->data[index] = value;
}

size_t pau_vector_get(PAU_Vector *vec, size_t index)
{
    if (!vec || index > vec->size)
        return -1;
    return vec->data[index];
}

// ============= 新增：迭代器核心函数 =============
// 1. 获取首迭代器（对应 QVector::begin()）
// 返回指向第一个元素的迭代器
PAU_VectorIter pau_vector_begin(PAU_Vector *vec)
{
    PAU_VectorIter iter = {0};
    if (vec && vec->size > 0)
    {
        iter.vec = vec;
        iter.current = (size_t *)(vec->data) + 1; // 指向第一个元素
    }
    return iter;
}

// 2. 获取尾后迭代器（对应 QVector::end()）
// 返回指向“最后一个元素的下一个位置”的迭代器（不指向有效元素）
PAU_VectorIter pau_vector_end(PAU_Vector *vec)
{
    PAU_VectorIter iter = {0};
    if (vec)
    {
        iter.vec = vec;
        // 尾后位置 = 数组起始地址 + 元素数量 * 元素大小
        iter.current = (size_t *)vec->data + 1 + vec->size;
    }
    return iter;
}

// 3. 迭代器向后移动（对应 ++it）
// 返回 0 成功，-1 失败（如已到 end 位置）
int pau_vector_iter_next(PAU_VectorIter *iter)
{
    if (!iter || !iter->vec || !iter->current)
        return -1;

    // 计算下一个元素的地址
    void *next = (size_t *)iter->current + 1;
    // 检查是否超过 end 位置
    if (next > (size_t *)pau_vector_end(iter->vec).current)
    {
        return -1;
    }
    iter->current = next;
    return 0;
}

// 4. 迭代器向前移动（对应 --it）
// 返回 0 成功，-1 失败（如已到 begin 位置）
int pau_vector_iter_prev(PAU_VectorIter *iter)
{
    if (!iter || !iter->vec || !iter->current)
        return -1;

    // 计算上一个元素的地址
    void *prev = (size_t *)iter->current - 1;
    // 检查是否低于 begin 位置
    if (prev < (size_t *)pau_vector_begin(iter->vec).current)
    {
        return -1;
    }
    iter->current = prev;
    return 0;
}

// 5. 迭代器解引用（对应 *it）
// 返回当前元素的指针，NULL 表示无效（如 end 位置）
void *pau_vector_iter_get(PAU_VectorIter *iter)
{
    if (!iter || !iter->vec || !iter->current)
        return NULL;

    // 检查是否是 end 位置（尾后位置无有效元素）
    if (iter->current > (size_t *)pau_vector_end(iter->vec).current)
    {
        return NULL;
    }
    return iter->current;
}

// 6. 判断两个迭代器是否相等（对应 it1 == it2）
// 返回 1 相等，0 不相等
bool pau_vector_iter_equal(PAU_VectorIter *iter1, PAU_VectorIter *iter2)
{
    if (!iter1 || !iter2 || iter1->vec != iter2->vec)
    {
        return 0; // 迭代器不属于同一个 PAU_Vector，必然不相等
    }
    return (iter1->current == iter2->current);
}
