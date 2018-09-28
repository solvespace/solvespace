//
// Created by benjamin on 9/25/18.
//

#ifndef SOLVESPACE_LIST_H
#define SOLVESPACE_LIST_H

#include <memory>
#include "platform.h"
#include "ss_util.h"

namespace SolveSpace {

// A simple list
template <class T>
class List {
        public:
        T   *elem;
        int  n;
        int  elemsAllocated;

        void ReserveMore(int howMuch) {
            if(n + howMuch > elemsAllocated) {
                elemsAllocated = n + howMuch;
                T *newElem = (T *)MemAlloc((size_t)elemsAllocated*sizeof(elem[0]));
                for(int i = 0; i < n; i++) {
                    new(&newElem[i]) T(std::move(elem[i]));
                    elem[i].~T();
                }
                MemFree(elem);
                elem = newElem;
            }
        }

        void AllocForOneMore() {
            if(n >= elemsAllocated) {
                ReserveMore((elemsAllocated + 32)*2 - n);
            }
        }

        void Add(const T *t) {
            AllocForOneMore();
            new(&elem[n++]) T(*t);
        }

        void AddToBeginning(const T *t) {
            AllocForOneMore();
            new(&elem[n]) T();
            std::move_backward(elem, elem + 1, elem + n + 1);
            elem[0] = *t;
            n++;
        }

        T *First() {
            return (n == 0) ? NULL : &(elem[0]);
        }
        const T *First() const {
            return (n == 0) ? NULL : &(elem[0]);
        }
        T *NextAfter(T *prev) {
            if(!prev) return NULL;
            if(prev - elem == (n - 1)) return NULL;
            return prev + 1;
        }
        const T *NextAfter(const T *prev) const {
            if(!prev) return NULL;
            if(prev - elem == (n - 1)) return NULL;
            return prev + 1;
        }

        T *begin() { return &elem[0]; }
        T *end() { return &elem[n]; }
        const T *begin() const { return &elem[0]; }
        const T *end() const { return &elem[n]; }

        void ClearTags() {
            int i;
            for(i = 0; i < n; i++) {
                elem[i].tag = 0;
            }
        }

        void Clear() {
            for(int i = 0; i < n; i++)
                elem[i].~T();
            if(elem) MemFree(elem);
            elem = NULL;
            n = elemsAllocated = 0;
        }

        void RemoveTagged() {
            int src, dest;
            dest = 0;
            for(src = 0; src < n; src++) {
                if(elem[src].tag) {
                    // this item should be deleted
                } else {
                    if(src != dest) {
                        elem[dest] = elem[src];
                    }
                    dest++;
                }
            }
            for(int i = dest; i < n; i++)
                elem[i].~T();
            n = dest;
            // and elemsAllocated is untouched, because we didn't resize
        }

        void RemoveLast(int cnt) {
            ssassert(n >= cnt, "Removing more elements than the list contains");
            for(int i = n - cnt; i < n; i++)
                elem[i].~T();
            n -= cnt;
            // and elemsAllocated is untouched, same as in RemoveTagged
        }

        void Reverse() {
            int i;
            for(i = 0; i < (n/2); i++) {
                swap(elem[i], elem[(n-1)-i]);
            }
        }
};

// A list, where each element has an integer identifier. The list is kept
// sorted by that identifier, and items can be looked up in log n time by
// id.
template <class T, class H>
class IdList {
        public:
        T     *elem;
        int   n;
        int   elemsAllocated;

        uint32_t MaximumId() {
            if(n == 0) {
                return 0;
            } else {
                return elem[n - 1].h.v;
            }
        }

        H AddAndAssignId(T *t) {
            t->h.v = (MaximumId() + 1);
            Add(t);

            return t->h;
        }

        void ReserveMore(int howMuch) {
            if(n + howMuch > elemsAllocated) {
                elemsAllocated = n + howMuch;
                T *newElem = (T *)MemAlloc((size_t)elemsAllocated*sizeof(elem[0]));
                for(int i = 0; i < n; i++) {
                    new(&newElem[i]) T(std::move(elem[i]));
                    elem[i].~T();
                }
                MemFree(elem);
                elem = newElem;
            }
        }

        void Add(T *t) {
            if(n >= elemsAllocated) {
                ReserveMore((elemsAllocated + 32)*2 - n);
            }

            int first = 0, last = n;
            // We know that we must insert within the closed interval [first,last]
            while(first != last) {
                int mid = (first + last)/2;
                H hm = elem[mid].h;
                ssassert(hm.v != t->h.v, "Handle isn't unique");
                if(hm.v > t->h.v) {
                    last = mid;
                } else if(hm.v < t->h.v) {
                    first = mid + 1;
                }
            }

            int i = first;
            new(&elem[n]) T();
            std::move_backward(elem + i, elem + n, elem + n + 1);
            elem[i] = *t;
            n++;
        }

        T *FindById(H h) {
            T *t = FindByIdNoOops(h);
            ssassert(t != NULL, "Cannot find handle");
            return t;
        }

        int IndexOf(H h) {
            int first = 0, last = n-1;
            while(first <= last) {
                int mid = (first + last)/2;
                H hm = elem[mid].h;
                if(hm.v > h.v) {
                    last = mid-1; // and first stays the same
                } else if(hm.v < h.v) {
                    first = mid+1; // and last stays the same
                } else {
                    return mid;
                }
            }
            return -1;
        }

        T *FindByIdNoOops(H h) {
            int first = 0, last = n-1;
            while(first <= last) {
                int mid = (first + last)/2;
                H hm = elem[mid].h;
                if(hm.v > h.v) {
                    last = mid-1; // and first stays the same
                } else if(hm.v < h.v) {
                    first = mid+1; // and last stays the same
                } else {
                    return &(elem[mid]);
                }
            }
            return NULL;
        }

        T *First() {
            return (n == 0) ? NULL : &(elem[0]);
        }
        T *NextAfter(T *prev) {
            if(!prev) return NULL;
            if(prev - elem == (n - 1)) return NULL;
            return prev + 1;
        }

        T *begin() { return &elem[0]; }
        T *end() { return &elem[n]; }
        const T *begin() const { return &elem[0]; }
        const T *end() const { return &elem[n]; }

        void ClearTags() {
            int i;
            for(i = 0; i < n; i++) {
                elem[i].tag = 0;
            }
        }

        void Tag(H h, int tag) {
            int i;
            for(i = 0; i < n; i++) {
                if(elem[i].h.v == h.v) {
                    elem[i].tag = tag;
                }
            }
        }

        void RemoveTagged() {
            int src, dest;
            dest = 0;
            for(src = 0; src < n; src++) {
                if(elem[src].tag) {
                    // this item should be deleted
                    elem[src].Clear();
                } else {
                    if(src != dest) {
                        elem[dest] = elem[src];
                    }
                    dest++;
                }
            }
            for(int i = dest; i < n; i++)
                elem[i].~T();
            n = dest;
            // and elemsAllocated is untouched, because we didn't resize
        }
        void RemoveById(H h) {
            ClearTags();
            FindById(h)->tag = 1;
            RemoveTagged();
        }

        void MoveSelfInto(IdList<T,H> *l) {
            l->Clear();
            *l = *this;
            elemsAllocated = n = 0;
            elem = NULL;
        }

        void DeepCopyInto(IdList<T,H> *l) {
            l->Clear();
            l->elem = (T *)MemAlloc(elemsAllocated * sizeof(elem[0]));
            for(int i = 0; i < n; i++)
                new(&l->elem[i]) T(elem[i]);
            l->elemsAllocated = elemsAllocated;
            l->n = n;
        }

        void Clear() {
            for(int i = 0; i < n; i++) {
                elem[i].Clear();
                elem[i].~T();
            }
            elemsAllocated = n = 0;
            if(elem) MemFree(elem);
            elem = NULL;
        }
};

} // namespace SolveSpace

#endif //SOLVESPACE_LIST_H
