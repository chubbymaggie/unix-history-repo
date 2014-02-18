//===-- QueueList.h --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_QueueList_h_
#define liblldb_QueueList_h_

#include <vector>

#include "lldb/lldb-private.h"
#include "lldb/Core/UserID.h"
#include "lldb/Utility/Iterable.h"


namespace lldb_private {

//------------------------------------------------------------------
// QueueList:
// This is the container for libdispatch aka Grand Central Dispatch 
// Queue objects.
//
// Each Process will have a QueueList.  When the process execution is
// paused, the QueueList may be populated with Queues by the 
// SystemRuntime.
//------------------------------------------------------------------

class QueueList
{
friend class Process;

public:

    QueueList (Process *process);

    ~QueueList ();

    //------------------------------------------------------------------
    /// Get the number of libdispatch queues that are available
    ///
    /// @return
    ///     The number of queues that are stored in the QueueList.
    //------------------------------------------------------------------
    uint32_t
    GetSize();

    //------------------------------------------------------------------
    /// Get the Queue at a given index number
    ///
    /// @param [in] idx
    ///     The index number (0-based) of the queue.
    /// @return
    ///     The Queue at that index number.
    //------------------------------------------------------------------
    lldb::QueueSP
    GetQueueAtIndex (uint32_t idx);

    typedef std::vector<lldb::QueueSP> collection;
    typedef LockingAdaptedIterable<collection, lldb::QueueSP, vector_adapter> QueueIterable;

    //------------------------------------------------------------------
    /// Iterate over the list of queues
    ///
    /// @return
    ///     An Iterable object which can be used to loop over the queues
    ///     that exist.
    //------------------------------------------------------------------
    QueueIterable
    Queues ()
    {
        return QueueIterable(m_queues, m_mutex);
    }

    //------------------------------------------------------------------
    /// Clear out the list of queues from the QueueList
    //------------------------------------------------------------------
    void
    Clear();

    //------------------------------------------------------------------
    /// Add a Queue to the QueueList
    ///
    /// @param [in] queue
    ///     Used by the SystemRuntime to populate the QueueList
    //------------------------------------------------------------------
    void
    AddQueue (lldb::QueueSP queue);

    //------------------------------------------------------------------
    /// Find a queue in the QueueList by QueueID
    ///
    /// @param [in] qid
    ///     The QueueID (same as returned by Thread::GetQueueID()) to find.
    ///
    /// @return
    ///     A QueueSP to the queue requested, if it is present in the QueueList.
    ///     An empty QueueSP willbe returned if this queue was not found.
    //------------------------------------------------------------------
    lldb::QueueSP
    FindQueueByID (lldb::queue_id_t qid);

    //------------------------------------------------------------------
    /// Find a queue in the QueueList by IndexID
    ///
    /// @param [in] index_id
    ///     Find a queue by IndexID.  This is an integer associated with each
    ///     unique queue seen during a debug session and will not be reused
    ///     for a different queue.  Unlike the QueueID, a 64-bit value, this
    ///     will tend to be an integral value like 1 or 7.
    ///
    /// @return
    ///     A QueueSP to the queue requested, if it is present in the QueueList.
    ///     An empty QueueSP willbe returned if this queue was not found.
    //------------------------------------------------------------------
    lldb::QueueSP
    FindQueueByIndexID (uint32_t index_id);

    lldb_private::Mutex &
    GetMutex ();

protected:

    //------------------------------------------------------------------
    // Classes that inherit from Process can see and modify these
    //------------------------------------------------------------------
    Process *m_process; ///< The process that manages this queue list.
    uint32_t m_stop_id; ///< The process stop ID that this queue list is valid for.
    collection m_queues; ///< The queues for this process.
    Mutex   m_mutex;

private:
    QueueList ();
};

} // namespace lldb_private

#endif  // liblldb_QueueList_h_
