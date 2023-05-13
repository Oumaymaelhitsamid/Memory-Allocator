/******************************************************
 * Copyright Grégory Mounié 2018                      *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <stdint.h>
#include <assert.h>
#include "mem.h"
#include "mem_internals.h"

unsigned int puiss2(unsigned long size) {
    unsigned int p=0;
    size = size -1; // allocation start in 0
    while(size) {  // get the largest bit
	p++;
	size >>= 1;
    }
    if (size > (1 << p))
	p++;
    return p;
}

void cree_bloc(unsigned int taille_bloc) {
    // On a atteint l'indice maximal sans avoir trouvé de bloc libre
    if (taille_bloc == FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant) {
        mem_realloc_medium();
        return ;
    }

    // Pour créer un bloc de cette taille, on a besoin d'au moins un bloc de la taille supérieure
    if (arena.TZL[taille_bloc +1] == NULL) {
        cree_bloc(taille_bloc +1);
    }

    // On récupère le bloc de taille supérieure
    void* bloc_sup = arena.TZL[taille_bloc +1];
    void** A = bloc_sup;
    arena.TZL[taille_bloc +1] = *A;

    // On le divise en deux
    void* bloc1 = bloc_sup;
    void* bloc2 = bloc_sup + (0b1 << taille_bloc);
    
    // On les insère dans la liste
    arena.TZL[taille_bloc] = bloc1;

    A = bloc1;
    *A = (void*) bloc2;

    A = bloc2;
    *A = (void*) 0;

    // NOTE : Le bloc2 ne peut pas avoir de suivant, sinon, on n'aurait pas pu atteindre cet endroit par les appels successifs
    //        (On aurait déjà trouvé un bloc de cette taille donc pas besoin de subdivision d'un plus gros bloc)
}


void* emalloc_medium(unsigned long size) {
    assert(size < LARGEALLOC);
    assert(size > SMALLALLOC);

    unsigned long taille = size + 32;

    unsigned int taille_bloc = puiss2(taille);

    if (arena.TZL[taille_bloc] == NULL) {
        cree_bloc(taille_bloc);
    }    

    // On récupère la tête de la liste et on décale la liste des tailles
    void* ptr = arena.TZL[taille_bloc];
    void** A = ptr;
    arena.TZL[taille_bloc] = *A;

    // On initialise le chunk qu'on vient de récupérer
    void* usr_ptr = mark_memarea_and_get_user_ptr(ptr, taille, MEDIUM_KIND);

    return usr_ptr;
}

void fusion_bloc(unsigned int taille_bloc, void* ptr, void* buddy) {

    void** A;
    void** suiv;

    // On a fusionné le maximum (éviter un accès interdit)
    if (taille_bloc == FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant) {
        A = NULL;
    } else {
        A = arena.TZL[taille_bloc];
    }

    while (A != NULL) {
        
        suiv = *A;
        if (suiv == buddy) {
            // On enlève le buddy de la liste
            suiv = *suiv;
            *A = suiv;

            // On récupère le pointeur qui va gérer le bloc de taille supérieure
            ptr = ((unsigned long) ptr < (unsigned long) buddy) ? ptr : buddy;

            // On va chercher à fusionner le bloc que l'on vient d'obtenir
            void* buddy = (void*) (((unsigned long) ptr) ^ (0b1 << (taille_bloc +1)));
            fusion_bloc(taille_bloc +1, ptr, buddy);
            return ;
        }

        A = suiv;
    }

    // On place le bloc dans la liste puisqu'on ne peut pas le fusionner (le buddy n'est pas libre)
    A = ptr;
    *A = (void*) arena.TZL[taille_bloc];
    arena.TZL[taille_bloc] = ptr;
}

void efree_medium(Alloc a) {
    
    unsigned int taille_bloc = puiss2(a.size);
    void* buddy = (void*) (((unsigned long) a.ptr) ^ (0b1 << taille_bloc));

    fusion_bloc(taille_bloc, a.ptr, buddy);
}


