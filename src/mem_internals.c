/******************************************************
 * Copyright Grégory Mounié 2018                      *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <sys/mman.h>
#include <assert.h>
#include <stdint.h>
#include "mem.h"
#include "mem_internals.h"

unsigned long knuth_mmix_one_round(unsigned long in)
{
    return in * 6364136223846793005UL % 1442695040888963407UL;
}

void *mark_memarea_and_get_user_ptr(void* ptr, unsigned long size, MemKind k) {

    unsigned long* tmp_ptr = ptr; // COmme c'est un long*, on avancera bien de 8 octets avec ++
    long magic = (knuth_mmix_one_round((unsigned long) ptr) & ~(0x3)) | k;

    // On écrit les bornes au début
    *tmp_ptr = size;
    tmp_ptr++;
    *tmp_ptr = magic;
    tmp_ptr++;

    // On passe la taille du bloc
    tmp_ptr = (unsigned long*) (((char*) tmp_ptr) + (size - 32));

    // On écrit les bornes à la fin
    *tmp_ptr = magic;
    tmp_ptr++;
    *tmp_ptr = size;

    return ptr + 16; // Comme c'est un void*, on avance réellement de 1 octet par ++
}

Alloc mark_check_and_get_alloc(void *ptr) {
    
    Alloc a = {};

    unsigned long* tmp_ptr = ptr;
    tmp_ptr -= 2;

    // On récupère les bornes
    a.ptr = (void*) tmp_ptr;
    a.size = *tmp_ptr;
    tmp_ptr++;

    long magic = *tmp_ptr;
    a.kind = magic & 0x3UL;
    tmp_ptr++;

    // Verification de la cohérence de la valeur magique
    long expected_magic = (knuth_mmix_one_round((unsigned long) a.ptr) & ~(0x3)) | a.kind;
    assert(magic == expected_magic);

    // On vérifie que la taille du bloc correspond 
    assert(!(a.size >= LARGEALLOC && a.kind != LARGE_KIND));   // Le bloc est trop petit pour la taille voulue
    assert(!(a.size <= SMALLALLOC && a.kind != SMALL_KIND));   // Le bloc est trop grand pour la taille voulue

    // On vérifie la cohérence des bornes avec celles à la fin du bloc
    tmp_ptr = (unsigned long*) (((char*) tmp_ptr) + (a.size - 32));
    long magic_end = *tmp_ptr;
    tmp_ptr++;
    long size_end = *tmp_ptr;

    assert(magic == magic_end);     // Les bornes magic ne correspondent pas
    assert(a.size == size_end);     // Les bornes taille ne correspondent pas

    return a;
}


unsigned long
mem_realloc_small() {
    assert(arena.chunkpool == 0);
    unsigned long size = (FIRST_ALLOC_SMALL << arena.small_next_exponant);
    arena.chunkpool = mmap(0,
			   size,
			   PROT_READ | PROT_WRITE | PROT_EXEC,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   -1,
			   0);
    if (arena.chunkpool == MAP_FAILED)
	handle_fatalError("small realloc");
    arena.small_next_exponant++;
    return size;
}

unsigned long
mem_realloc_medium() {
    uint32_t indice = FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant;
    assert(arena.TZL[indice] == 0);
    unsigned long size = (FIRST_ALLOC_MEDIUM << arena.medium_next_exponant);
    assert( size == (1 << indice));
    arena.TZL[indice] = mmap(0,
			     size*2, // twice the size to allign
			     PROT_READ | PROT_WRITE | PROT_EXEC,
			     MAP_PRIVATE | MAP_ANONYMOUS,
			     -1,
			     0);
    if (arena.TZL[indice] == MAP_FAILED)
	handle_fatalError("medium realloc");
    // align allocation to a multiple of the size
    // for buddy algo
    arena.TZL[indice] += (size - (((intptr_t)arena.TZL[indice]) % size));
    arena.medium_next_exponant++;
    return size; // lie on allocation size, but never free
}


// used for test in buddy algo
unsigned int
nb_TZL_entries() {
    int nb = 0;
    
    for(int i=0; i < TZL_SIZE; i++)
	if ( arena.TZL[i] )
	    nb ++;

    return nb;
}
