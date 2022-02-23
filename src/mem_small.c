/******************************************************
 * Copyright Grégory Mounié 2018                      *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <assert.h>
#include "mem.h"
#include "mem_internals.h"

void* emalloc_small(unsigned long size) {
    
    if (arena.chunkpool == NULL) {
        // On réalloue des cases
        unsigned long taille_chunk = mem_realloc_small();
        unsigned long nb_chunks = taille_chunk / CHUNKSIZE;

        void** A;

        for (unsigned int i = 0; i < nb_chunks-1; i++) {
            void* cible = arena.chunkpool + (i+1)*CHUNKSIZE;
            void* adresse = arena.chunkpool + i*CHUNKSIZE;

            // Chaque case contient un pointeur vers le suivant
            A = adresse;
            *A = (void*) cible;
        }

        // La dernière case contient (donc pointe vers) le NULL
        A = arena.chunkpool + (nb_chunks-1)*CHUNKSIZE;
        *A = NULL;
    }

    // On récupère la tête de la liste et on décale le chunkpool
    void* ptr = arena.chunkpool;
    void** A = ptr;
    arena.chunkpool = *A;

    // On initialise le chunk qu'on vient de récupérer
    void* usr_ptr = mark_memarea_and_get_user_ptr(ptr, CHUNKSIZE, SMALL_KIND);

    return usr_ptr;
}

void efree_small(Alloc a) {
    
    void* cible = arena.chunkpool;
    void* adresse = a.ptr;

    // On fait pointer la case récupérée vers le chunkpool actuel
    void** A = adresse;
    *A = (void*) cible;

    // On fait pointer le chunkpool vers la case récupérée
    arena.chunkpool = a.ptr;
}
