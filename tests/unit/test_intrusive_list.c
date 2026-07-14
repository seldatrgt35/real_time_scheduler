#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "intrusive_list.h"

static int test_failures;
static unsigned int assertion_count;

#define CHECK(condition)                    \
    do                                      \
    {                                       \
        if (!(condition))                   \
        {                                   \
            ++test_failures;                \
        }                                   \
    } while (0)

void rts_assert_fail(const char *expression, const char *file, int line)
{
    (void)expression;
    (void)file;
    (void)line;
    ++assertion_count;
}

static bool list_is_valid(const rts_list_t *list)
{
    const rts_list_node_t *node;
    const rts_list_node_t *previous = NULL;
    const rts_list_node_t *next = NULL;
    size_t forward_count = 0u;
    size_t backward_count = 0u;

    if (list == NULL)
    {
        return false;
    }

    if (list->count == 0u)
    {
        return list->head == NULL && list->tail == NULL;
    }

    if (list->head == NULL || list->tail == NULL ||
        list->head->previous != NULL || list->tail->next != NULL)
    {
        return false;
    }

    node = list->head;
    while (node != NULL && forward_count <= list->count)
    {
        if (node->owner != list || node->previous != previous)
        {
            return false;
        }
        previous = node;
        node = node->next;
        ++forward_count;
    }

    if (node != NULL || previous != list->tail || forward_count != list->count)
    {
        return false;
    }

    node = list->tail;
    while (node != NULL && backward_count <= list->count)
    {
        if (node->owner != list || node->next != next)
        {
            return false;
        }
        next = node;
        node = node->previous;
        ++backward_count;
    }

    return node == NULL && next == list->head &&
           backward_count == list->count;
}

static void initialize_nodes(rts_list_node_t *nodes, size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index)
    {
        rts_list_node_initialize(&nodes[index]);
    }
}

static void test_initialization(void)
{
    rts_list_t list;
    rts_list_node_t node;

    rts_list_initialize(&list);
    rts_list_node_initialize(&node);

    CHECK(rts_list_is_empty(&list));
    CHECK(list.count == 0u);
    CHECK(list_is_valid(&list));
    CHECK(!rts_list_node_is_linked(&node));
    CHECK(node.previous == NULL && node.next == NULL && node.owner == NULL &&
          node.object == NULL);
}

static void test_push_back_order(void)
{
    rts_list_t list;
    rts_list_node_t nodes[3];

    rts_list_initialize(&list);
    initialize_nodes(nodes, 3u);

    rts_list_push_back(&list, &nodes[0]);
    CHECK(list.head == &nodes[0] && list.tail == &nodes[0]);
    CHECK(nodes[0].previous == NULL && nodes[0].next == NULL);
    CHECK(nodes[0].owner == &list && list.count == 1u);
    CHECK(list_is_valid(&list));

    rts_list_push_back(&list, &nodes[1]);
    rts_list_push_back(&list, &nodes[2]);
    CHECK(list.head == &nodes[0] && list.tail == &nodes[2]);
    CHECK(nodes[0].next == &nodes[1]);
    CHECK(nodes[1].previous == &nodes[0] && nodes[1].next == &nodes[2]);
    CHECK(nodes[2].previous == &nodes[1]);
    CHECK(list.count == 3u && list_is_valid(&list));
}

static void test_insert_before(void)
{
    rts_list_t list;
    rts_list_node_t nodes[4];

    rts_list_initialize(&list);
    initialize_nodes(nodes, 4u);
    rts_list_push_back(&list, &nodes[0]);
    rts_list_push_back(&list, &nodes[2]);
    rts_list_push_back(&list, &nodes[3]);

    rts_list_insert_before(&list, &nodes[2], &nodes[1]);
    CHECK(nodes[0].next == &nodes[1]);
    CHECK(nodes[1].previous == &nodes[0] && nodes[1].next == &nodes[2]);
    CHECK(nodes[2].previous == &nodes[1]);
    CHECK(list.count == 4u && list_is_valid(&list));

    rts_list_remove(&list, &nodes[0]);
    rts_list_insert_before(&list, list.head, &nodes[0]);
    CHECK(list.head == &nodes[0]);
    CHECK(nodes[0].previous == NULL && nodes[0].next == &nodes[1]);
    CHECK(list.count == 4u && list_is_valid(&list));
}

static void test_remove_positions(void)
{
    rts_list_t list;
    rts_list_node_t nodes[4];

    rts_list_initialize(&list);
    initialize_nodes(nodes, 4u);
    nodes[1].object = &nodes[1];
    rts_list_push_back(&list, &nodes[0]);
    rts_list_push_back(&list, &nodes[1]);
    rts_list_push_back(&list, &nodes[2]);
    rts_list_push_back(&list, &nodes[3]);

    rts_list_remove(&list, &nodes[1]);
    CHECK(nodes[0].next == &nodes[2] && nodes[2].previous == &nodes[0]);
    CHECK(!rts_list_node_is_linked(&nodes[1]));
    CHECK(nodes[1].object == &nodes[1]);
    CHECK(list.count == 3u && list_is_valid(&list));

    rts_list_remove(&list, &nodes[0]);
    CHECK(list.head == &nodes[2] && nodes[2].previous == NULL);
    CHECK(list.count == 2u && list_is_valid(&list));

    rts_list_remove(&list, &nodes[3]);
    CHECK(list.tail == &nodes[2] && nodes[2].next == NULL);
    CHECK(list.count == 1u && list_is_valid(&list));

    rts_list_remove(&list, &nodes[2]);
    CHECK(rts_list_is_empty(&list));
    CHECK(!rts_list_node_is_linked(&nodes[2]));
    CHECK(nodes[2].previous == NULL && nodes[2].next == NULL &&
          nodes[2].owner == NULL);
    CHECK(list_is_valid(&list));
}

static void test_independent_lists(void)
{
    rts_list_t first;
    rts_list_t second;
    rts_list_node_t nodes[2];

    rts_list_initialize(&first);
    rts_list_initialize(&second);
    initialize_nodes(nodes, 2u);
    rts_list_push_back(&first, &nodes[0]);
    rts_list_push_back(&second, &nodes[1]);

    CHECK(nodes[0].owner == &first && nodes[1].owner == &second);
    CHECK(first.count == 1u && second.count == 1u);
    CHECK(list_is_valid(&first) && list_is_valid(&second));
}

static void test_contract_assertions(void)
{
#if RTS_ENABLE_ASSERTIONS
    rts_list_t first;
    rts_list_t second;
    rts_list_node_t linked;
    rts_list_node_t unlinked;
    rts_list_node_t second_position;
    rts_list_node_t overflow_member;
    rts_list_node_t overflow_candidate;
    rts_list_node_t underflow_node;
    rts_list_t overflow_list;
    rts_list_t underflow_list;
    unsigned int before;

    rts_list_initialize(&first);
    rts_list_initialize(&second);
    rts_list_node_initialize(&linked);
    rts_list_node_initialize(&unlinked);
    rts_list_node_initialize(&second_position);
    rts_list_node_initialize(&overflow_member);
    rts_list_node_initialize(&overflow_candidate);
    rts_list_node_initialize(&underflow_node);
    rts_list_push_back(&first, &linked);
    rts_list_push_back(&second, &second_position);

    before = assertion_count;
    rts_list_push_back(&first, &linked);
    CHECK(assertion_count == before + 1u);
    CHECK(first.count == 1u && linked.owner == &first && list_is_valid(&first));

    before = assertion_count;
    rts_list_remove(&second, &linked);
    CHECK(assertion_count == before + 1u);
    CHECK(first.count == 1u && second.count == 1u && list_is_valid(&first));

    before = assertion_count;
    rts_list_insert_before(&first, &second_position, &unlinked);
    CHECK(assertion_count == before + 1u);
    CHECK(!rts_list_node_is_linked(&unlinked));

    before = assertion_count;
    rts_list_remove(&first, &unlinked);
    CHECK(assertion_count == before + 1u);
    CHECK(!rts_list_node_is_linked(&unlinked));
    CHECK(first.count == 1u && list_is_valid(&first));

    rts_list_initialize(&overflow_list);
    rts_list_push_back(&overflow_list, &overflow_member);
    overflow_list.count = SIZE_MAX;
    before = assertion_count;
    rts_list_push_back(&overflow_list, &overflow_candidate);
    CHECK(assertion_count == before + 1u);
    CHECK(!rts_list_node_is_linked(&overflow_candidate));
    overflow_list.count = 1u;
    CHECK(list_is_valid(&overflow_list));

    rts_list_initialize(&underflow_list);
    underflow_node.owner = &underflow_list;
    before = assertion_count;
    rts_list_remove(&underflow_list, &underflow_node);
    CHECK(assertion_count == before + 1u);
    CHECK(underflow_list.count == 0u);
    rts_list_node_initialize(&underflow_node);
#endif
}

int rts_test_intrusive_list_run(void)
{
    test_initialization();
    test_push_back_order();
    test_insert_before();
    test_remove_positions();
    test_independent_lists();
    test_contract_assertions();
    return test_failures;
}

int main(void)
{
    return rts_test_intrusive_list_run();
}
