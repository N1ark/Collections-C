/*
 * Collections-C
 * Copyright (C) 2013-2014 Srđan Panić <srdja.panic@gmail.com>
 *
 * This file is part of Collections-C.
 *
 * Collections-C is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Collections-C is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Collections-C.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "include/cc_array.h"

#define DEFAULT_CAPACITY 8
#define DEFAULT_EXPANSION_FACTOR 2

struct cc_array_s {
    size_t   size;
    size_t   capacity;
    float    exp_factor;
    void   **buffer;
};

static enum cc_stat expand_capacity(CC_Array *ar);


/**
 * Creates a new empty CC_Array based on the specified CC_ArrayConf struct and
 * returns a status code.
 *
 * The CC_Array is allocated using the allocators specified in the CC_ArrayConf
 * struct. The allocation may fail if underlying allocator fails. It may also
 * fail if the values of exp_factor and capacity in the CC_ArrayConf do not meet
 * the following condition: <code>exp_factor < (CC_MAX_ELEMENTS / capacity)</code>.
 *
 * @param[in] conf array configuration structure
 * @param[out] out pointer to where the newly created CC_Array is to be stored
 *
 * @return CC_OK if the creation was successful, CC_ERR_INVALID_CAPACITY if
 * the above mentioned condition is not met, or CC_ERR_ALLOC if the memory
 * allocation for the new CC_Array structure failed.
 */
enum cc_stat cc_array_new(CC_Array **out)
{
    float ex = DEFAULT_EXPANSION_FACTOR;
    CC_Array *ar = calloc(1, sizeof(CC_Array));

    if (!ar)
        return CC_ERR_ALLOC;

    void **buff = malloc(DEFAULT_CAPACITY * sizeof(void*));

    if (!buff) {
        free(ar);
        return CC_ERR_ALLOC;
    }

    ar->buffer     = buff;
    ar->exp_factor = ex;
    ar->capacity   = DEFAULT_CAPACITY;

    *out = ar;
    return CC_OK;
}

/**
 * Destroys the CC_Array structure, but leaves the data it used to hold intact.
 *
 * @param[in] ar the array that is to be destroyed
 */
void cc_array_destroy(CC_Array *ar)
{
    free(ar->buffer);
    free(ar);
}

/**
 * Adds a new element to the CC_Array. The element is appended to the array making
 * it the last element (the one with the highest index) of the CC_Array.
 *
 * @param[in] ar the array to which the element is being added
 * @param[in] element the element that is being added
 *
 * @return CC_OK if the element was successfully added, CC_ERR_ALLOC if the
 * memory allocation for the new element failed, or CC_ERR_MAX_CAPACITY if the
 * array is already at maximum capacity.
 */
enum cc_stat cc_array_add(CC_Array *ar, void *element)
{
    if (ar->size >= ar->capacity) {
        enum cc_stat status = expand_capacity(ar);
        if (status != CC_OK)
            return status;
    }

    ar->buffer[ar->size] = element;
    ar->size++;

    return CC_OK;
}

/**
 * Adds a new element to the array at a specified position by shifting all
 * subsequent elements by one. The specified index must be within the bounds
 * of the array. This function may also fail if the memory allocation for
 * the new element was unsuccessful.
 *
 * @param[in] ar the array to which the element is being added
 * @param[in] element the element that is being added
 * @param[in] index the position in the array at which the element is being
 *            added
 *
 * @return CC_OK if the element was successfully added, CC_ERR_OUT_OF_RANGE if
 * the specified index was not in range, CC_ERR_ALLOC if the memory
 * allocation for the new element failed, or CC_ERR_MAX_CAPACITY if the
 * array is already at maximum capacity.
 */
enum cc_stat cc_array_add_at(CC_Array *ar, void *element, size_t index)
{
    if (index == ar->size)
        return cc_array_add(ar, element);

    if ((ar->size == 0 && index != 0) || index > (ar->size - 1))
        return CC_ERR_OUT_OF_RANGE;

    if (ar->size >= ar->capacity) {
        enum cc_stat status = expand_capacity(ar);
        if (status != CC_OK)
            return status;
    }

    size_t shift = (ar->size - index) * sizeof(void*);

    memmove(&(ar->buffer[index + 1]),
            &(ar->buffer[index]),
            shift);

    ar->buffer[index] = element;
    ar->size++;

    return CC_OK;
}

/**
 * Replaces an array element at the specified index and optionally sets the out
 * parameter to the value of the replaced element. The specified index must be
 * within the bounds of the CC_Array.
 *
 * @param[in]  ar      array whose element is being replaced
 * @param[in]  element replacement element
 * @param[in]  index   index at which the replacement element should be inserted
 * @param[out] out     pointer to where the replaced element is stored, or NULL if
 *                     it is to be ignored
 *
 * @return CC_OK if the element was successfully replaced, or CC_ERR_OUT_OF_RANGE
 *         if the index was out of range.
 */
enum cc_stat cc_array_replace_at(CC_Array *ar, void *element, size_t index, void **out)
{
    if (index >= ar->size)
        return CC_ERR_OUT_OF_RANGE;

    if (out)
        *out = ar->buffer[index];

    ar->buffer[index] = element;

    return CC_OK;
}

enum cc_stat cc_array_swap_at(CC_Array *ar, size_t index1, size_t index2)
{
    void *tmp;

    if (index1 >= ar->size || index2 >= ar->size)
        return CC_ERR_OUT_OF_RANGE;

    tmp = ar->buffer[index1];

    ar->buffer[index1] = ar->buffer[index2];
    ar->buffer[index2] = tmp;
    return CC_OK;
}

/**
 * Removes the specified element from the CC_Array if such element exists and
 * optionally sets the out parameter to the value of the removed element.
 *
 * @param[in] ar array from which the element is being removed
 * @param[in] element element being removed
 * @param[out] out pointer to where the removed value is stored, or NULL
 *                 if it is to be ignored
 *
 * @return CC_OK if the element was successfully removed, or
 * CC_ERR_VALUE_NOT_FOUND if the element was not found.
 */
enum cc_stat cc_array_remove(CC_Array *ar, void *element, void **out)
{
    size_t index;
    enum cc_stat status = cc_array_index_of(ar, element, &index);

    if (status == CC_ERR_OUT_OF_RANGE)
        return CC_ERR_VALUE_NOT_FOUND;

    if (index != ar->size - 1) {
        size_t block_size = (ar->size - 1 - index) * sizeof(void*);

        memmove(&(ar->buffer[index]),
                &(ar->buffer[index + 1]),
                block_size);
    }
    ar->size--;

    if (out)
        *out = element;

    return CC_OK;
}

/**
 * Removes an CC_Array element from the specified index and optionally sets the
 * out parameter to the value of the removed element. The index must be within
 * the bounds of the array.
 *
 * @param[in] ar the array from which the element is being removed
 * @param[in] index the index of the element being removed.
 * @param[out] out  pointer to where the removed value is stored,
 *                  or NULL if it is to be ignored
 *
 * @return CC_OK if the element was successfully removed, or CC_ERR_OUT_OF_RANGE
 * if the index was out of range.
 */
enum cc_stat cc_array_remove_at(CC_Array *ar, size_t index, void **out)
{
    if (index >= ar->size)
        return CC_ERR_OUT_OF_RANGE;

    if (out)
        *out = ar->buffer[index];

    if (index != ar->size - 1) {
        size_t block_size = (ar->size - 1 - index) * sizeof(void*);

        memmove(&(ar->buffer[index]),
                &(ar->buffer[index + 1]),
                block_size);
    }
    ar->size--;

    return CC_OK;
}

/**
 * Removes all elements from the specified array. This function does not shrink
 * the array capacity.
 *
 * @param[in] ar array from which all elements are to be removed
 */
void cc_array_remove_all(CC_Array *ar)
{
    ar->size = 0;
}

/**
 * Removes and frees all elements from the specified array. This function does
 * not shrink the array capacity.
 *
 * @param[in] ar array from which all elements are to be removed
 */
void cc_array_remove_all_free(CC_Array *ar)
{
    size_t i;
    for (i = 0; i < ar->size; i++)
        free(ar->buffer[i]);

    cc_array_remove_all(ar);
}

/**
 * Gets an CC_Array element from the specified index and sets the out parameter to
 * its value. The specified index must be within the bounds of the array.
 *
 * @param[in] ar the array from which the element is being retrieved
 * @param[in] index the index of the array element
 * @param[out] out pointer to where the element is stored
 *
 * @return CC_OK if the element was found, or CC_ERR_OUT_OF_RANGE if the index
 * was out of range.
 */
enum cc_stat cc_array_get_at(CC_Array *ar, size_t index, void **out)
{
    if (index >= ar->size)
        return CC_ERR_OUT_OF_RANGE;

    *out = ar->buffer[index];
    return CC_OK;
}

/**
 * Gets the index of the specified element. The returned index is the index
 * of the first occurrence of the element starting from the beginning of the
 * CC_Array.
 *
 * @param[in] ar array being searched
 * @param[in] element the element whose index is being looked up
 * @param[out] index  pointer to where the index is stored
 *
 * @return CC_OK if the index was found, or CC_OUT_OF_RANGE if not.
 */
enum cc_stat cc_array_index_of(CC_Array *ar, void *element, size_t *index)
{
    size_t i;
    for (i = 0; i < ar->size; i++) {
        if (ar->buffer[i] == element) {
            *index = i;
            return CC_OK;
        }
    }
    return CC_ERR_OUT_OF_RANGE;
}

/**
 * Creates a subarray of the specified CC_Array, ranging from <code>b</code>
 * index (inclusive) to <code>e</code> index (inclusive). The range indices
 * must be within the bounds of the CC_Array, while the <code>e</code> index
 * must be greater or equal to the <code>b</code> index.
 *
 * @note The new CC_Array is allocated using the original CC_Array's allocators
 *       and it also inherits the configuration of the original CC_Array.
 *
 * @param[in] ar array from which the subarray is being created
 * @param[in] b the beginning index (inclusive) of the subarray that must be
 *              within the bounds of the array and must not exceed the
 *              the end index
 * @param[in] e the end index (inclusive) of the subarray that must be within
 *              the bounds of the array and must be greater or equal to the
 *              beginning index
 * @param[out] out pointer to where the new sublist is stored
 *
 * @return CC_OK if the subarray was successfully created, CC_ERR_INVALID_RANGE
 * if the specified index range is invalid, or CC_ERR_ALLOC if the memory allocation
 * for the new subarray failed.
 */
enum cc_stat cc_array_subarray(CC_Array *ar, size_t b, size_t e, CC_Array **out)
{
    if (b > e || e >= ar->size)
        return CC_ERR_INVALID_RANGE;

    CC_Array *sub_ar = calloc(1, sizeof(CC_Array));

    if (!sub_ar)
        return CC_ERR_ALLOC;

    /* Try to allocate the buffer */
    if (!(sub_ar->buffer = malloc(ar->capacity * sizeof(void*)))) {
        free(sub_ar);
        return CC_ERR_ALLOC;
    }

    sub_ar->size       = e - b + 1;
    sub_ar->capacity   = sub_ar->size;

    memcpy(sub_ar->buffer,
           &(ar->buffer[b]),
           sub_ar->size * sizeof(void*));

    *out = sub_ar;
    return CC_OK;
}

/**
 * Creates a shallow copy of the specified CC_Array. A shallow copy is a copy of
 * the CC_Array structure, but not the elements it holds.
 *
 * @note The new CC_Array is allocated using the original CC_Array's allocators
 *       and it also inherits the configuration of the original array.
 *
 * @param[in] ar the array to be copied
 * @param[out] out pointer to where the newly created copy is stored
 *
 * @return CC_OK if the copy was successfully created, or CC_ERR_ALLOC if the
 * memory allocation for the copy failed.
 */
enum cc_stat cc_array_copy_shallow(CC_Array *ar, CC_Array **out)
{
    CC_Array *copy = malloc(sizeof(CC_Array));

    if (!copy)
        return CC_ERR_ALLOC;

    if (!(copy->buffer = calloc(ar->capacity, sizeof(void*)))) {
        free(copy);
        return CC_ERR_ALLOC;
    }
    copy->exp_factor = ar->exp_factor;
    copy->size       = ar->size;
    copy->capacity   = ar->capacity;

    memcpy(copy->buffer,
           ar->buffer,
           copy->size * sizeof(void*));

    *out = copy;
    return CC_OK;
}

/**
 * Reverses the order of elements in the specified array.
 *
 * @param[in] ar array that is being reversed
 */
void cc_array_reverse(CC_Array *ar)
{
    if (ar->size == 0)
        return;

    size_t i;
    size_t j;
    for (i = 0, j = ar->size - 1; i < ar->size / 2; i++, j--) {
        void *tmp = ar->buffer[i];
        ar->buffer[i] = ar->buffer[j];
        ar->buffer[j] = tmp;
    }
}

/**
 * Trims the array's capacity, in other words, it shrinks the capacity to match
 * the number of elements in the CC_Array, however the capacity will never shrink
 * below 1.
 *
 * @param[in] ar array whose capacity is being trimmed
 *
 * @return CC_OK if the capacity was trimmed successfully, or CC_ERR_ALLOC if
 * the reallocation failed.
 */
enum cc_stat cc_array_trim_capacity(CC_Array *ar)
{
    if (ar->size == ar->capacity)
        return CC_OK;

    void **new_buff = calloc(ar->size, sizeof(void*));

    if (!new_buff)
        return CC_ERR_ALLOC;

    size_t size = ar->size < 1 ? 1 : ar->size;

    memcpy(new_buff, ar->buffer, size * sizeof(void*));
    free(ar->buffer);

    ar->buffer   = new_buff;
    ar->capacity = ar->size;

    return CC_OK;
}

/**
 * Returns the number of occurrences of the element within the specified CC_Array.
 *
 * @param[in] ar array that is being searched
 * @param[in] element the element that is being searched for
 *
 * @return the number of occurrences of the element.
 */
size_t cc_array_contains(CC_Array *ar, void *element)
{
    size_t o = 0;
    size_t i;
    for (i = 0; i < ar->size; i++) {
        if (ar->buffer[i] == element)
            o++;
    }
    return o;
}

/**
 * Returns the number of occurrences of the value pointed to by <code>e</code>
 * within the specified CC_Array.
 *
 * @param[in] ar array that is being searched
 * @param[in] element the element that is being searched for
 * @param[in] cmp comparator function which returns 0 if the values passed to it are equal
 *
 * @return the number of occurrences of the value.
 */
size_t cc_array_contains_value(CC_Array *ar, void *element, int (*cmp) (const void*, const void*))
{
    size_t o = 0;
    size_t i;
    for (i = 0; i < ar->size; i++) {
        if (cmp(element, ar->buffer[i]) == 0)
            o++;
    }
    return o;
}

/**
 * Returns the size of the specified CC_Array. The size of the array is the
 * number of elements contained within the CC_Array.
 *
 * @param[in] ar array whose size is being returned
 *
 * @return the the number of element within the CC_Array.
 */
size_t cc_array_size(CC_Array *ar)
{
    return ar->size;
}

/**
 * Returns the capacity of the specified CC_Array. The capacity of the CC_Array is
 * the maximum number of elements an CC_Array can hold before it has to be resized.
 *
 * @param[in] ar array whose capacity is being returned
 *
 * @return the capacity of the CC_Array.
 */
size_t cc_array_capacity(CC_Array *ar)
{
    return ar->capacity;
}

/**
 * Expands the CC_Array capacity. This might fail if the the new buffer
 * cannot be allocated. In case the expansion would overflow the index
 * range, a maximum capacity buffer is allocated instead. If the capacity
 * is already at the maximum capacity, no new buffer is allocated.
 *
 * @param[in] ar array whose capacity is being expanded
 *
 * @return CC_OK if the buffer was expanded successfully, CC_ERR_ALLOC if
 * the memory allocation for the new buffer failed, or CC_ERR_MAX_CAPACITY
 * if the array is already at maximum capacity.
 */
static enum cc_stat expand_capacity(CC_Array *ar)
{
    if (ar->capacity == CC_MAX_ELEMENTS)
        return CC_ERR_MAX_CAPACITY;

    size_t new_capacity = (size_t)(ar->capacity * ar->exp_factor);

    /* As long as the capacity is greater that the expansion factor
     * at the point of overflow, this is check is valid. */
    if (new_capacity <= ar->capacity)
        ar->capacity = CC_MAX_ELEMENTS;
    else
        ar->capacity = new_capacity;

    void **new_buff = malloc(ar->capacity * sizeof(void*));

    if (!new_buff)
        return CC_ERR_ALLOC;

    memcpy(new_buff, ar->buffer, ar->size * sizeof(void*));

    free(ar->buffer);
    ar->buffer = new_buff;

    return CC_OK;
}
