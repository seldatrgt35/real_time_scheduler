#ifndef RTS_INTRUSIVE_LIST_H
#define RTS_INTRUSIVE_LIST_H

#include <stdbool.h>
#include <stddef.h>

#include "rts/rts_types.h"

struct rts_list;

typedef struct rts_list_node
{
    struct rts_list_node *previous;
    struct rts_list_node *next;
    struct rts_list *owner;
    /* Opaque association owned by the embedding data-structure module. */
    void *object;
} rts_list_node_t;

typedef struct rts_list
{
    rts_list_node_t *head;
    rts_list_node_t *tail;
    size_t count;
} rts_list_t;

void rts_list_initialize(rts_list_t *list);
void rts_list_node_initialize(rts_list_node_t *node);
bool rts_list_is_empty(const rts_list_t *list);
bool rts_list_node_is_linked(const rts_list_node_t *node);
void rts_list_push_back(rts_list_t *list, rts_list_node_t *node);
void rts_list_insert_before(rts_list_t *list,
                            rts_list_node_t *position,
                            rts_list_node_t *node);
void rts_list_remove(rts_list_t *list, rts_list_node_t *node);

#endif /* RTS_INTRUSIVE_LIST_H */
