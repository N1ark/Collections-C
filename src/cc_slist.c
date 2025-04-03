/*
 * Collections-C
 * Copyright (C) 2013-2015 Srđan Panić <srdja.panic@gmail.com>
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

#include "include/cc_slist.h"


struct cc_slist_s {
    size_t  size;
    SNode   *head;
    SNode   *tail;
};


static void* unlinkn             (CC_SList *list, SNode *node, SNode *prev);
static bool  unlinkn_all         (CC_SList *list, void (*cb) (void*));
static enum cc_stat get_node_at  (CC_SList *list, size_t index, SNode **node, SNode **prev);
static enum cc_stat get_node     (CC_SList *list, void *element, SNode **node, SNode **prev);

/**
 * Creates a new empty CC_SList based on the specified CC_SListConf struct and
 * returns a status code.
 *
 * The CC_SList is allocated using the allocators specified in the CC_SListConf
 * struct. The allocation may fail if the underlying allocator fails.
 *
 * @param[in] conf CC_SList configuration struct. All fields must be initialized
 *            to appropriate values.
 *
 * @param[out] out Pointer to a CC_SList that is being createdo
 *
 * @return CC_OK if the creation was successful, or CC_ERR_ALLOC if the
 * memory allocation for the new CC_SList structure failed.
 */
enum cc_stat cc_slist_new(CC_SList **out)
{
    CC_SList *list = calloc(1, sizeof(CC_SList));

    if (!list)
        return CC_ERR_ALLOC;

    *out = list;
    return CC_OK;
}

/**
 * Destroys the list structure, but leaves the data that is holds intact.
 *
 * @param[in] list CC_SList that is to be destroyed
 */
void cc_slist_destroy(CC_SList *list)
{
    cc_slist_remove_all(list);
    free(list);
}

/**
 * Destroys the list structure along with all the data it holds.
 *
 * @note
 * This function should not be called on a list that has some of it's elements
 * allocated on the stack.
 *
 * @param[in] list CC_SList that is to be destroyed
 */
void cc_slist_destroy_cb(CC_SList *list, void (*cb) (void*))
{
    cc_slist_remove_all_cb(list, cb);
    free(list);
}

/**
 * Prepends a new element to the list (adds a new "head") making it the first
 * element of the list.
 *
 * @param[in] list CC_SList to which the element is being added
 * @param[in] element element that is being added
 *
 * @return CC_OK if the element was successfully added, or CC_ERR_ALLOC if the
 * memory allocation for the new element has failed.
 */
enum cc_stat cc_slist_add_first(CC_SList *list, void *element)
{
    SNode *node = calloc(1, sizeof(SNode));

    if (!node)
        return CC_ERR_ALLOC;

    node->data = element;

    if (list->size == 0) {
        list->head = node;
        list->tail = node;
    } else {
        node->next = list->head;
        list->head = node;
    }
    list->size++;
    return CC_OK;
}

/**
 * Appends a new element to the list (adds a new "tail") making it the last
 * element of the list.
 *
 * @param[in] list CC_SList to which the element is being added
 * @param[in] element element that is being added
 *
 * @return CC_OK if the element was successfully added, or CC_ERR_ALLOC if the
 * memory allocation for the new element has failed.
 */
enum cc_stat cc_slist_add_last(CC_SList *list, void *element)
{
    SNode *node = calloc(1, sizeof(SNode));

    if (!node)
        return CC_ERR_ALLOC;

    node->data = element;

    if (list->size == 0) {
        list->head       = node;
        list->tail       = node;
    } else {
        list->tail->next = node;
        list->tail       = node;
    }
    list->size++;
    return CC_OK;
}

/**
 * Adds a new element at the specified location in the CC_SList and shifts all
 * subsequent elements by one. The index at which the new element is being
 * added must be within the bounds of the list.
 *
 * @note This operation cannot be performed on an empty list.
 *
 * @param[in] list CC_SList to which this element is being added
 * @param[in] element element that is being added
 * @param[in] index the position in the list at which the new element is being
 *                  added
 *
 * @return CC_OK if the element was successfully added, CC_ERR_OUT_OF_RANGE if
 * the specified index was not in range, or CC_ERR_ALLOC if the memory
 * allocation for the new element failed.
 */
enum cc_stat cc_slist_add_at(CC_SList *list, void *element, size_t index)
{
    SNode *prev = NULL;
    SNode *node = NULL;

    enum cc_stat status = get_node_at(list, index, &node, &prev);

    if (status != CC_OK)
        return status;

    SNode *new = calloc(1, sizeof(SNode));

    if (!new)
        return CC_ERR_ALLOC;

    new->data = element;

    if (!prev) {
        new->next  = list->head;
        list->head = new;
    } else {
        SNode *tmp = prev->next;
        prev->next = new;
        new->next  = tmp;
    }

    list->size++;
    return CC_OK;
}

/**
 * Splices the two CC_SLists together by appending the second list to the
 * first. This function moves all the elements from the second list into
 * the first list, leaving the second list empty.
 *
 * @param[in] list1 The consumer list to which the elements are moved.
 * @param[in] list2 The producer list from which the elements are moved.
 *
 * @return CC_OK if the elements were successfully moved
 */
enum cc_stat cc_slist_splice(CC_SList *list1, CC_SList *list2)
{
    if (list2->size == 0)
        return CC_OK;

    if (list1->size == 0) {
        list1->head = list2->head;
        list1->tail = list2->tail;
    } else {
        list1->tail->next = list2->head;
        list1->tail = list2->tail;
    }
    list1->size += list2->size;

    list2->head = NULL;
    list2->tail = NULL;
    list2->size = 0;

    return CC_OK;
}

/**
 * Removes the first occurrence of the element from the specified CC_SList
 * and optionally sets the out parameter to the value of the removed element.
 *
 * @param[in] list CC_SList from which the element is being removed
 * @param[in] element element that is being removed
 * @param[out] out Pointer to where the removed value is stored, or NULL
 *                 if it is to be ignored
 *
 * @return CC_OK if the element was successfully removed, or
 * CC_ERR_VALUE_NOT_FOUND if the element was not found.
 */
enum cc_stat cc_slist_remove(CC_SList *list, void *element, void **out)
{
    SNode *prev = NULL;
    SNode *node = NULL;

    enum cc_stat status = get_node(list, element, &node, &prev);

    if (status != CC_OK)
        return status;

    void *val = unlinkn(list, node, prev);

    if (out)
        *out = val;

    return CC_OK;
}

/**
 * Removes the element at the specified index and optionally sets
 * the out parameter to the value of the removed element. The index
 * must be within the bounds of the list.
 *
 * @param[in] list  CC_SList from which the element is being removed
 * @param[in] index Index of the element that is being removed. Must be be
 *                  within the index range of the list.
 * @param[out] out  Pointer to where the removed value is stored,
 *                  or NULL if it is to be ignored
 *
 * @return CC_OK if the element was successfully removed, or CC_ERR_OUT_OF_RANGE
 * if the index was out of range.
 */
enum cc_stat cc_slist_remove_at(CC_SList *list, size_t index, void **out)
{
    SNode *prev = NULL;
    SNode *node = NULL;

    enum cc_stat status = get_node_at(list, index, &node, &prev);

    if (status != CC_OK)
        return status;

    void *e = unlinkn(list, node, prev);

    if (out)
        *out = e;

    return CC_OK;
}

/**
 * Removes the first (head) element of the list and optionally sets the out
 * parameter to the value of the removed element.
 *
 * @param[in] list CC_SList from which the first element is being removed
 * @param[out] out Pointer to where the removed value is stored, or NULL if it is
 *                 to be ignored
 *
 * @return CC_OK if the element was successfully removed, or CC_ERR_VALUE_NOT_FOUND
 * if the list is empty.
 */
enum cc_stat cc_slist_remove_first(CC_SList *list, void **out)
{
    if (list->size == 0)
        return CC_ERR_VALUE_NOT_FOUND;

    void *e = unlinkn(list, list->head, NULL);

    if (out)
        *out = e;

    return CC_OK;
}

/**
 * Removes all elements from the specified list.
 *
 * @param[in] list CC_SList from which all elements are being removed
 *
 * @return CC_OK if the elements were successfully removed, or CC_ERR_VALUE_NOT_FOUND
 * if the list was already empty.
 */
enum cc_stat cc_slist_remove_all(CC_SList *list)
{
    bool unlinked = unlinkn_all(list, NULL);

    if (unlinked) {
        list->head = NULL;
        list->tail = NULL;
        return CC_OK;
    }
    return CC_ERR_VALUE_NOT_FOUND;
}

/**
 * Removes and frees all the elements from the specified list.
 *
 * @note
 * This function should not be called on a list that has some of it's elements
 * allocated on the stack.
 *
 * @param[in] list CC_SList from which all the elements are being removed and freed
 *
 * @return CC_OK if the element were successfully removed and freed, or
 * CC_ERR_VALUE_NOT_FOUND if the list was already empty.
 */
enum cc_stat cc_slist_remove_all_cb(CC_SList *list, void (*cb) (void*))
{
    bool unlinked = unlinkn_all(list, cb);

    if (unlinked) {
        list->head = NULL;
        list->tail = NULL;
        return CC_OK;
    }
    return CC_ERR_VALUE_NOT_FOUND;
}

/**
 * Replaces an element at the specified location and optionally sets the out parameter
 * to the value of the replaced element. The specified index must be within the bounds
 * of the list.
 *
 * @param[in] list    CC_SList on which this operation is performed
 * @param[in] element the replacement element
 * @param[in] index   index of the element being replaced
 * @param[out] out    Pointer to where the replaced element is stored, or NULL if
 *                    it is to be ignored
 *
 * @return CC_OK if the element was successfully replaced, or CC_ERR_OUT_OF_RANGE
 *         if the index was out of range.
 */
enum cc_stat cc_slist_replace_at(CC_SList *list, void *element, size_t index, void **out)
{
    SNode *prev = NULL;
    SNode *node = NULL;

    enum cc_stat status = get_node_at(list, index, &node, &prev);

    if (status != CC_OK)
        return status;

    void *old = node->data;
    node->data = element;

    if (out)
        *out = old;

    return CC_OK;
}

/**
 * Gets the list element from the specified index and sets the out parameter to
 * its value.
 *
 * @param[in] list  CC_SList from which the element is being returned.
 * @param[in] index The index of a list element being returned. The index must
 *                  be within the bound of the list.
 * @param[out] out  Pointer to where the element is stored
 *
 * @return CC_OK if the element was found, or CC_ERR_OUT_OF_RANGE if the index
 * was out of range.
 */
enum cc_stat cc_slist_get_at(CC_SList *list, size_t index, void **out)
{
    SNode *prev = NULL;
    SNode *node = NULL;

    enum cc_stat status = get_node_at(list, index, &node, &prev);

    if (status != CC_OK)
        return status;

    *out = node->data;

    return CC_OK;
}

/**
 * Returns the number of elements in the specified CC_SList.
 *
 * @param[in] list CC_SList whose size is being returned
 *
 * @return The number of elements contained in the specified CC_SList.
 */
size_t cc_slist_size(CC_SList *list)
{
    return list->size;
}

/**
 * Reverses the order of elements in the specified list.
 *
 * @param[in] list CC_SList that is being reversed
 */
void cc_slist_reverse(CC_SList *list)
{
    if (list->size == 0 || list->size == 1)
        return;

    SNode *prev = NULL;
    SNode *next = NULL;
    SNode *flip = list->head;

    list->tail = list->head;

    while (flip) {
        next = flip->next;
        flip->next = prev;
        prev = flip;
        flip = next;
    }
    list->head = prev;
}

/**
 * Creates a shallow copy of the specified list. A shallow copy is a copy of the
 * list structure. This operation does not copy the actual data that this list
 * holds.
 *
 * @note The new list is allocated using the original lists allocators and also
 *       inherits the configuration of the original list.
 *
 * @param[in] list CC_SList to be copied
 * @param[out] out Pointer to where the newly created copy is stored
 *
 * @return CC_OK if the copy was successfully created, or CC_ERR_ALLOC if the
 * memory allocation for the copy failed.
 */
enum cc_stat cc_slist_copy_shallow(CC_SList *list, CC_SList **out)
{
    CC_SList *copy;
    enum cc_stat status = cc_slist_new(&copy);

    if (status != CC_OK)
        return status;

    SNode *node = list->head;

    while (node) {
        status = cc_slist_add_last(copy, node->data);
        if (status != CC_OK) {
            cc_slist_destroy(copy);
            return status;
        }
        node = node->next;
    }
    *out = copy;
    return CC_OK;
}

/**
 * Returns an integer representing the number of occurrences of the specified
 * element within the CC_SList.
 *
 * @param[in] list CC_SList on which the search is performed
 * @param[in] element element being searched for
 *
 * @return number of found matches
 */
size_t cc_slist_contains(CC_SList *list, void *element)
{
    SNode *node = list->head;
    size_t e_count = 0;

    while (node) {
        if (node->data == element)
            e_count++;
        node = node->next;
    }
    return e_count;
}

/**
 * Returns the number of occurrences of the value pointed to by <code>element</code>
 * within the specified CC_SList.
 *
 * @param[in] list CC_SList on which the search is performed
 * @param[in] element element being searched for
 * @param[in] cmp Comparator function which returns 0 if the values passed to it are equal
 *
 * @return number of occurrences of the value
 */
size_t cc_slist_contains_value(CC_SList *list, void *element, int (*cmp) (const void*, const void*))
{
    SNode *node = list->head;
    size_t e_count = 0;

    while (node) {
        if (cmp(node->data, element) == 0)
            e_count++;
        node = node->next;
    }
    return e_count;
}



/**
 * Unlinks the node from the list and returns the data that was associated with it and
 * also adjusts the head / tail of the list if necessary.
 *
 * @param[in] list the list from which the node is being unlinked
 * @parma[in] node the node being unlinked
 * @param[in] prev the node that immediately precedes the node that is being unlinked
 *
 * @return the data that was at this node
 */
static void *unlinkn(CC_SList *list, SNode *node, SNode *prev)
{
    void *data = node->data;

    if (prev)
        prev->next = node->next;
    else
        list->head = node->next;

    if (!node->next)
        list->tail = prev;

    free(node);
    list->size--;

    return data;
}

/**
 * Unlinks all nodes from the list and optionally frees the data at the nodes.
 *
 * @param[in] list the list from which all nodes are being unlinked
 * @param[in] freed specified whether or not the data at the nodes should also
 *                  be deallocated.
 *
 * @return false if the list is already y empty, otherwise returns true
 */
static bool unlinkn_all(CC_SList *list, void (*cb) (void*))
{
    if (list->size == 0)
        return false;

    SNode *n = list->head;

    while (n) {
        SNode *tmp = n->next;

        if (cb)
            cb(n->data);

        free(n);
        n = tmp;
        list->size--;
    }
    return true;
}

/**
 * Finds the node at the specified index. If the index is not in the bounds
 * of the list, NULL is returned instead.
 *
 * @param[in] list the list from which the node is being returned
 * @param[in] index the index of the node
 * @param[out] node the node at the specified index
 * @param[out] prev the node that immediately precedes the node at the
 *                  specified index
 *
 * @return CC_OK if the node was found, or CC_ERR_OUT_OF_RANGE if the index
 * was out of range.
 */
static enum cc_stat
get_node_at(CC_SList *list, size_t index, SNode **node, SNode **prev)
{
    if (index >= list->size)
        return CC_ERR_OUT_OF_RANGE;

    *node = list->head;
    *prev = NULL;

    size_t i;
    for (i = 0; i < index; i++) {
        *prev = *node;
        *node = (*node)->next;
    }
    return CC_OK;
}

/**
 * Finds the first node from the beginning of the list that is associated
 * with the specified element. If no node is associated with the element,
 * NULL is returned instead.
 *
 * @param[in] list the list from which the node is being returned
 * @param[in] element the element whose list node is being returned
 * @param[out] node the node associated with the data
 * @param[out] prev the node that immediately precedes the node at the
 *                  specified index
 *
 * @return CC_OK if the node was found, or CC_ERR_VALUE_NOT_FOUND if not.
 */
static enum cc_stat
get_node(CC_SList *list, void *element, SNode **node, SNode **prev)
{
   *node = list->head;
   *prev = NULL;

    while (*node) {
        if ((*node)->data == element)
            return CC_OK;

        *prev = *node;
        *node = (*node)->next;
    }
    return CC_ERR_VALUE_NOT_FOUND;
}
