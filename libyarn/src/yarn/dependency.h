/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Interface for dependency checking.

Note that we don't yet support unaligned reads and writes. We don't because it can become
very difficult to track these types of things.
*/

#ifndef YARN_DEPENDENCY_H_
#define YARN_DEPENDENCY_H_


#include "types.h"

bool yarn_dep_global_init (size_t ws_size, yarn_word_t index_size);
bool yarn_dep_global_reset (size_t ws_size, yarn_word_t index_size);
void yarn_dep_global_destroy (void);

bool yarn_dep_thread_init (yarn_word_t pool_id, yarn_word_t epoch);
void yarn_dep_thread_destroy (yarn_word_t pool_id);

bool yarn_dep_store (yarn_word_t pool_id, const void* src, void* dest);
bool yarn_dep_store_fast (yarn_word_t pool_id, 
			  yarn_word_t index_id, 
			  const void* src, 
			  void* dest);

bool yarn_dep_load (yarn_word_t pool_id, const void* src, void* dest);
bool yarn_dep_load_fast (yarn_word_t pool_id, 
			 yarn_word_t index_id, 
			 const void* src, 
			 void* dest);

void yarn_dep_commit (yarn_word_t epoch);
void yarn_dep_rollback (yarn_word_t epoch);


#endif // YARN_DEPENDENCY_H_
