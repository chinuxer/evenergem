#ifndef PAU_VECTOR_H
#define PAU_VECTOR_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stdarg.h>
#include <ctype.h>
#include "inttypes.h"
#include "math.h"
#include <float.h>
#include <limits.h>

// 默认初始容量
#if defined(QTDEMOD_NODES_CAPACITY)
#define PAU_VECTOR_DEFAULT_CAPACITY QTDEMOD_NODES_CAPACITY
#else
#define PAU_VECTOR_DEFAULT_CAPACITY 10
#endif

typedef size_t ID_TYPE;

// ------------- PAU_Vector 结构体 -------------
typedef struct
{
    size_t size;   // 当前已存储的元素个数
    size_t data[]; // 数据
} PAU_Vector;

// ------------- 迭代器结构体 -------------
typedef struct
{
    PAU_Vector *vec; // 关联的向量
    void *current;   // 指向当前元素的指针
} PAU_VectorIter;

// =============  核心函数声明 =============

// 创建向量
PAU_Vector *pau_vector_create(size_t init_capacity);
// 复制PAU_Vector
PAU_Vector *pau_vector_copy(PAU_Vector *vec);
// 获取元素个数(对应QVector::size())
size_t pau_vector_size(PAU_Vector *vec);
// 尾部追加元素
int pau_vector_append(PAU_Vector *vec, size_t elem);
int pau_vector_append_deep(PAU_Vector *vec, const void *elem);

// 按下标获取元素指针（返回 -1 表示越界）
size_t pau_vector_at(PAU_Vector *vec, size_t index);

// 删除指定值的元素（仅删除第一个匹配的）
int pau_vector_remove(PAU_Vector *vec, size_t elem);

// 清空所有元素（保留容量）
void pau_vector_clear(PAU_Vector *vec);

// 填充所有元素为指定值
void pau_vector_fill(PAU_Vector *vec, size_t value);

// 销毁向量
void pau_vector_destroy(PAU_Vector *vec);

//
void pau_vector_set(PAU_Vector *vec, size_t index, size_t value);

// =============  迭代器函数声明 =============

PAU_VectorIter pau_vector_begin(PAU_Vector *vec);
PAU_VectorIter pau_vector_end(PAU_Vector *vec);
int pau_vector_iter_next(PAU_VectorIter *iter);
int pau_vector_iter_prev(PAU_VectorIter *iter);
void *pau_vector_iter_get(PAU_VectorIter *iter);
bool pau_vector_iter_equal(PAU_VectorIter *iter1, PAU_VectorIter *iter2);

// =============  查找函数声明 =============
// 自定义比较函数类型
typedef int (*PAU_VectorCompareFunc)(const void *elem1, const void *elem2);
bool pau_vector_contains(PAU_Vector *vec, size_t target);
int pau_vector_contains_deep(PAU_Vector *vec, const size_t *target);
int pau_vector_contains_ex(PAU_Vector *vec, const void *target, PAU_VectorCompareFunc cmp_func);

// =============  遍历宏 =============
// 用法：size_t val; PAU_VECTOR_FOREACH(val, vec) { printf("%c\n", val); }
#define PAU_VECTOR_FOREACH_DEEP(val, vec)                    \
    for (PAU_VectorIter _it = pau_vector_begin(vec);         \
         !pau_vector_iter_equal(&_it, &pau_vector_end(vec)); \
         pau_vector_iter_next(&_it))                         \
        for (size_t val, bool _once = true; _once && (val = *(size_t *)pau_vector_iter_get(&_it); _once = false)

#define PAU_VECTOR_FOREACH(val, vec)                      \
    for (size_t index = 1; index <= (vec)->size; index++) \
        for (size_t val = 1; (val > 0) && (val = (vec)->data[index]) > 0; val = 0)

#endif // PAU_VECTOR_H