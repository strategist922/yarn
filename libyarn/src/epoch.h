/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

Keeper of the one and only speculative timeline.

\warning yarn_epoch is not guaranteed to work correctly if 
\code yarn_tpool_size() <= sizeof(yarn_word_t)-1 \endcode. If we ever enter an
era of 30+ core CPUs then this class will need to be updated so that it works better.
 */


#ifndef YARN_EPOCH_H_
#define YARN_EPOCH_H_


#include <types.h>



enum yarn_epoch_status {
  //! Waiting to be executed.
  yarn_epoch_waiting = 1,
         
  //! Currently executing.
  yarn_epoch_executing = 2,

  //! Ready for commit.
  yarn_epoch_done = 3,             

  //! Rollback detected but CAN'T safely performed.
  yarn_epoch_pending_rollback = 4, 

  //! Rollback detected and CAN be safely performed.
  yarn_epoch_rollback = 5,

  //! Is committed and entry can be reused.
  yarn_epoch_commit = 6
};


//! \warning Not thread safe.
bool yarn_epoch_init(void);
//! \warning Not thread safe.
void yarn_epoch_destroy(void);

//! Bounds for the active epochs.
yarn_word_t yarn_epoch_first(void);
yarn_word_t yarn_epoch_last(void);

/*! 
Returns the next epoch that should be executed. This may either be a rollbacked epoch if
the status is to \c yarn_epoch_rollback or a newly generated epoch if the status is set to
\c yarn_epoch_waiting.
*/
yarn_word_t yarn_epoch_next(enum yarn_epoch_status* old_status);


/*!
Contrary to what the name might suggests, these don't do the actual commits and rollback.
They only update or prepare the epoch data structures.
*/
void yarn_epoch_do_rollback(yarn_word_t start);
bool yarn_epoch_get_next_commit(yarn_word_t* epoch, void** task, void** data);
void yarn_epoch_set_commit(yarn_word_t epoch);
bool yarn_epoch_set_executing(yarn_word_t epoch);
void yarn_epoch_set_done(yarn_word_t epoch);

//! Returns the status of the epoch.
enum yarn_epoch_status yarn_epoch_get_status (yarn_word_t epoch);

/*!
Usefull meta information to associate with the epoch. yarn_epoch doesn't manage these in 
any ways so it's up to the client to set and free them as nessecary.
*/
void* yarn_epoch_get_task (yarn_word_t epoch);
void yarn_epoch_set_task (yarn_word_t epoch, void* task);
void* yarn_epoch_get_data(yarn_word_t epoch);
void yarn_epoch_set_data(yarn_word_t epoch, void* data);


#endif // YARN_EPOCH_H_