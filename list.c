// Code adapted from @Leyxargon
//  Original source: https://github.com/Leyxargon/c-linked-list

#include "list.h"
#include "boat.h"

List *newList() {
    List *list = (List *)malloc(sizeof(List));
    if (list == NULL)
        return NULL;
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    return list;
}

Node *newNode(void *generic) {
    Node *pnt = (Node *)malloc(sizeof(Node));
    if (pnt == NULL)
        return NULL;
    pnt->n = generic;
    pnt->next = NULL;
    return pnt;
}

void *get(List *list, int index) {
    if (list == NULL || list->length == 0 || index >= list->length ||
        index < 0) {
        return NULL;
    }
    Node *current = list->head;
    for (int i = 0; i < index; i++) {
        current = current->next;
    }
    return current->n;
}

void headInsert(List *list, void *x) { /* Implemented by José Julio Suárez */
    Node *node = newNode(x);
    if (list->head != NULL) {
        node->next = list->head;
    }
    list->head = node;
    if (list->length == 0) {
        list->tail = node;
    }
    list->length++;
}

void tailInsert(List *list,
                void *generic) { /* Implemented by José Julio Suárez */
    Node *node = newNode(generic);
    if (list->tail != NULL) {
        list->tail->next = node;
    }
    list->tail = node;
    if (list->length == 0) {
        list->head = node;
    }
    list->length++;
}

void *pop(List *list, int index) { /* Implemented by José Julio Suárez */
    Node *extracted = extractNode(list, index);
    if (extracted != NULL) {
        void *value = extracted->n;
        free(extracted);
        return value;
    }
    return NULL;
}

Node *extractNode(List *list, int index) {
    if (list == NULL || list->length == 0 || index >= list->length ||
        index < 0) {
        return NULL;
    }

    Node *previous = NULL;
    Node *toRemove = list->head;

    if (index == 0) {
        if (list->head == NULL) { // Additional check for NULL head
            return NULL;
        }
        list->head = list->head->next;

        // If the list is now empty, also update tail
        if (list->head == NULL) {
            list->tail = NULL;
        }
    } else {
        for (int i = 1; i <= index; i++) {
            previous = toRemove;
            toRemove = toRemove->next;
        }
        previous->next = toRemove->next;

        // Update tail if we removed the last node
        if (toRemove == list->tail) {
            list->tail = previous;
        }
    }

    list->length--;
    toRemove->next = NULL;
    return toRemove;
}

void appendNode(List *list, Node *node) { /* Implemented by José Julio Suárez */
    if (list == NULL || node == NULL) {
        return;
    }
    if (list->length == 0) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->length++;
}

void appendList(List *target,
                List *source) { /* Implemented by José Julio Suárez */
    if (target == NULL || source == NULL) {
        return;
    }
    if (source->length == 0) {
        free(source);
        return;
    }
    if (target->length == 0) {
        target->head = source->head;
        target->tail = source->tail;
        target->length = source->length;
        free(source);
        return;
    }
    target->tail->next = source->head;
    target->tail = source->tail;
    target->length += source->length;
    free(source);
}

void deleteList(List *list) { /* Modified by José Julio Suárez */
    Node *current = list->head;
    Boat *currentBoat = NULL;
    while (current != NULL) {
        currentBoat = (Boat *)current->n;
        close(currentBoat->connfd);
        free(currentBoat->destination);
        free(currentBoat);
        Node *next = current->next;
        free(current);
        current = next;
    }
    free(list);
}

bool deleteBoat(List *queue, int boatId) {
    Node *previous = NULL;
    Node *current = queue->head;
    while (current != NULL) {
        Boat *currentBoat = (Boat *)current->n;
        if (currentBoat->id == boatId) {
            if (previous != NULL) {
                previous->next = current->next;
            } else {
                queue->head = current->next;
            }
            if (current->next == NULL) {
                queue->tail = previous; // Update tail if needed
            }
            queue->length--;

            free(currentBoat->destination);
            free(currentBoat);
            free(current);
            return true;
        }
        previous = current;
        current = current->next;
    }
    return false;
}