#include "intrusive_list.h"

#include <stdint.h>

#include "assert_internal.h"

#define RTS_LIST_REQUIRE_VOID(condition)      \
    do                                        \
    {                                         \
        if (!(condition))                     \
        {                                     \
            RTS_ASSERT(condition);            \
            return;                           \
        }                                     \
    } while (0)

#define RTS_LIST_REQUIRE_FALSE(condition)     \
    do                                        \
    {                                         \
        if (!(condition))                     \
        {                                     \
            RTS_ASSERT(condition);            \
            return false;                     \
        }                                     \
    } while (0)

static bool rts_list_endpoints_are_valid(const rts_list_t *list)
{
    if (list->count == 0u)
    {
        return list->head == NULL && list->tail == NULL;
    }

    return list->head != NULL &&
           list->tail != NULL &&
           list->head->previous == NULL &&
           list->tail->next == NULL &&
           list->head->owner == list &&
           list->tail->owner == list;
}

void rts_list_initialize(rts_list_t *list)
{
    RTS_LIST_REQUIRE_VOID(list != NULL);

    list->head = NULL;
    list->tail = NULL;
    list->count = 0u;
}

void rts_list_node_initialize(rts_list_node_t *node)
{
    RTS_LIST_REQUIRE_VOID(node != NULL);

    node->previous = NULL;
    node->next = NULL;
    node->owner = NULL;
    node->object = NULL;
}

bool rts_list_is_empty(const rts_list_t *list)
{
    RTS_LIST_REQUIRE_FALSE(list != NULL);
    RTS_LIST_REQUIRE_FALSE(rts_list_endpoints_are_valid(list));

    return list->count == 0u;
}

bool rts_list_node_is_linked(const rts_list_node_t *node)
{
    RTS_LIST_REQUIRE_FALSE(node != NULL);

    if (node->owner == NULL)
    {
        RTS_LIST_REQUIRE_FALSE(node->previous == NULL);
        RTS_LIST_REQUIRE_FALSE(node->next == NULL);
        return false;
    }

    return true;
}

void rts_list_push_back(rts_list_t *list, rts_list_node_t *node)
{
    RTS_LIST_REQUIRE_VOID(list != NULL);
    RTS_LIST_REQUIRE_VOID(node != NULL);
    RTS_LIST_REQUIRE_VOID(rts_list_endpoints_are_valid(list));
    RTS_LIST_REQUIRE_VOID(node->owner == NULL);
    RTS_LIST_REQUIRE_VOID(node->previous == NULL);
    RTS_LIST_REQUIRE_VOID(node->next == NULL);
    RTS_LIST_REQUIRE_VOID(list->count < SIZE_MAX);

    node->previous = list->tail;
    node->next = NULL;
    node->owner = list;

    if (list->tail == NULL)
    {
        list->head = node;
    }
    else
    {
        list->tail->next = node;
    }

    list->tail = node;
    ++list->count;

    RTS_ASSERT(rts_list_endpoints_are_valid(list));
}

void rts_list_insert_before(rts_list_t *list,
                            rts_list_node_t *position,
                            rts_list_node_t *node)
{
    rts_list_node_t *previous;

    RTS_LIST_REQUIRE_VOID(list != NULL);
    RTS_LIST_REQUIRE_VOID(position != NULL);
    RTS_LIST_REQUIRE_VOID(node != NULL);
    RTS_LIST_REQUIRE_VOID(rts_list_endpoints_are_valid(list));
    RTS_LIST_REQUIRE_VOID(position->owner == list);
    RTS_LIST_REQUIRE_VOID(node->owner == NULL);
    RTS_LIST_REQUIRE_VOID(node->previous == NULL);
    RTS_LIST_REQUIRE_VOID(node->next == NULL);
    RTS_LIST_REQUIRE_VOID(list->count < SIZE_MAX);

    previous = position->previous;
    if (previous == NULL)
    {
        RTS_LIST_REQUIRE_VOID(list->head == position);
    }
    else
    {
        RTS_LIST_REQUIRE_VOID(previous->owner == list);
        RTS_LIST_REQUIRE_VOID(previous->next == position);
    }

    node->previous = previous;
    node->next = position;
    node->owner = list;
    position->previous = node;

    if (previous == NULL)
    {
        list->head = node;
    }
    else
    {
        previous->next = node;
    }

    ++list->count;

    RTS_ASSERT(rts_list_endpoints_are_valid(list));
}

void rts_list_remove(rts_list_t *list, rts_list_node_t *node)
{
    rts_list_node_t *previous;
    rts_list_node_t *next;

    RTS_LIST_REQUIRE_VOID(list != NULL);
    RTS_LIST_REQUIRE_VOID(node != NULL);
    RTS_LIST_REQUIRE_VOID(rts_list_endpoints_are_valid(list));
    RTS_LIST_REQUIRE_VOID(node->owner == list);
    RTS_LIST_REQUIRE_VOID(list->count > 0u);

    previous = node->previous;
    next = node->next;

    if (previous == NULL)
    {
        RTS_LIST_REQUIRE_VOID(list->head == node);
    }
    else
    {
        RTS_LIST_REQUIRE_VOID(previous->owner == list);
        RTS_LIST_REQUIRE_VOID(previous->next == node);
    }

    if (next == NULL)
    {
        RTS_LIST_REQUIRE_VOID(list->tail == node);
    }
    else
    {
        RTS_LIST_REQUIRE_VOID(next->owner == list);
        RTS_LIST_REQUIRE_VOID(next->previous == node);
    }

    if (previous == NULL)
    {
        list->head = next;
    }
    else
    {
        previous->next = next;
    }

    if (next == NULL)
    {
        list->tail = previous;
    }
    else
    {
        next->previous = previous;
    }

    node->previous = NULL;
    node->next = NULL;
    node->owner = NULL;
    --list->count;

    RTS_ASSERT(rts_list_endpoints_are_valid(list));
}
