#include <stdio.h>

#include "model.h"
#include "action.h"
#include "tree.h"
#include "schedule.h"
#include "common.h"

#define INITIAL_THREAD_ID	0

class Backtrack {
public:
	Backtrack(ModelAction *d, action_list_t *t) {
		diverge = d;
		actionTrace = t;
		iter = actionTrace->begin();
	}
	ModelAction * get_diverge() { return diverge; }
	action_list_t * get_trace() { return actionTrace; }
	void advance_state() { iter++; }
	ModelAction * get_state() {
		return iter == actionTrace->end() ? NULL : *iter;
	}
private:
	ModelAction *diverge;
	action_list_t *actionTrace;
	/* points to position in actionTrace as we replay */
	action_list_t::iterator iter;
};

ModelChecker *model;

void free_action_list(action_list_t *list)
{
	action_list_t::iterator it;
	for (it = list->begin(); it != list->end(); it++)
		delete (*it);
	delete list;
}

ModelChecker::ModelChecker()
	:
	/* Initialize default scheduler */
	scheduler(new Scheduler()),
	/* First thread created will have id INITIAL_THREAD_ID */
	next_thread_id(INITIAL_THREAD_ID),
	used_sequence_numbers(0),

	num_executions(0),
	current_action(NULL),
	exploring(NULL),
	nextThread(THREAD_ID_T_NONE),
	action_trace(new action_list_t()),
	rootNode(new TreeNode()),
	currentNode(rootNode)
{
}

ModelChecker::~ModelChecker()
{
	std::map<int, class Thread *>::iterator it;
	for (it = thread_map.begin(); it != thread_map.end(); it++)
		delete (*it).second;
	thread_map.clear();

	free_action_list(action_trace);

	delete scheduler;
	delete rootNode;
}

void ModelChecker::reset_to_initial_state()
{
	DEBUG("+++ Resetting to initial state +++\n");
	std::map<int, class Thread *>::iterator it;
	for (it = thread_map.begin(); it != thread_map.end(); it++)
		delete (*it).second;
	thread_map.clear();
	action_trace = new action_list_t();
	currentNode = rootNode;
	current_action = NULL;
	next_thread_id = INITIAL_THREAD_ID;
	used_sequence_numbers = 0;
	/* scheduler reset ? */
}

thread_id_t ModelChecker::get_next_id()
{
	return next_thread_id++;
}

int ModelChecker::get_next_seq_num()
{
	return ++used_sequence_numbers;
}

Thread * ModelChecker::schedule_next_thread()
{
	Thread *t;
	if (nextThread == THREAD_ID_T_NONE)
		return NULL;
	t = thread_map[id_to_int(nextThread)];

	ASSERT(t != NULL);

	return t;
}

/*
 * get_next_replay_thread() - Choose the next thread in the replay sequence
 *
 * If we've reached the 'diverge' point, then we pick a thread from the
 *   backtracking set.
 * Otherwise, we simply return the next thread in the sequence.
 */
thread_id_t ModelChecker::get_next_replay_thread()
{
	ModelAction *next;
	thread_id_t tid;

	next = exploring->get_state();

	if (next == exploring->get_diverge()) {
		TreeNode *node = next->get_treenode();

		/* Reached divergence point; discard our current 'exploring' */
		DEBUG("*** Discard 'Backtrack' object ***\n");
		tid = node->getNextBacktrack();
		delete exploring;
		exploring = NULL;
	} else {
		tid = next->get_tid();
	}
	DEBUG("*** ModelChecker chose next thread = %d ***\n", tid);
	return tid;
}

thread_id_t ModelChecker::advance_backtracking_state()
{
	/* Have we completed exploring the preselected path? */
	if (exploring == NULL)
		return THREAD_ID_T_NONE;

	/* Else, we are trying to replay an execution */
	exploring->advance_state();

	ASSERT(exploring->get_state() != NULL);

	return get_next_replay_thread();
}

bool ModelChecker::next_execution()
{
	DBG();

	num_executions++;
	print_summary();
	if ((exploring = model->get_next_backtrack()) == NULL)
		return false;

	if (DBG_ENABLED()) {
		printf("Next execution will diverge at:\n");
		exploring->get_diverge()->print();
		print_list(exploring->get_trace());
	}

	model->reset_to_initial_state();
	nextThread = get_next_replay_thread();
	return true;
}

ModelAction * ModelChecker::get_last_conflict(ModelAction *act)
{
	action_type type = act->get_type();

	switch (type) {
		case THREAD_CREATE:
		case THREAD_YIELD:
		case THREAD_JOIN:
			return NULL;
		case ATOMIC_READ:
		case ATOMIC_WRITE:
		default:
			break;
	}
	/* linear search: from most recent to oldest */
	action_list_t::reverse_iterator rit;
	for (rit = action_trace->rbegin(); rit != action_trace->rend(); rit++) {
		ModelAction *prev = *rit;
		if (act->is_dependent(prev))
			return prev;
	}
	return NULL;
}

void ModelChecker::set_backtracking(ModelAction *act)
{
	ModelAction *prev;
	TreeNode *node;
	Thread *t = get_thread(act->get_tid());

	prev = get_last_conflict(act);
	if (prev == NULL)
		return;

	node = prev->get_treenode();

	while (t && !node->is_enabled(t))
		t = t->get_parent();

	/* Check if this has been explored already */
	if (node->hasBeenExplored(t->get_id()))
		return;
	/* If this is a new backtracking point, mark the tree */
	if (node->setBacktrack(t->get_id()) != 0)
		return;

	DEBUG("Setting backtrack: conflict = %d, instead tid = %d\n",
			prev->get_tid(), t->get_id());
	if (DBG_ENABLED()) {
		prev->print();
		act->print();
	}

	Backtrack *back = new Backtrack(prev, action_trace);
	backtrack_list.push_back(back);
}

Backtrack * ModelChecker::get_next_backtrack()
{
	Backtrack *next;
	if (backtrack_list.empty())
		return NULL;
	next = backtrack_list.back();
	backtrack_list.pop_back();
	return next;
}

void ModelChecker::check_current_action(void)
{
	ModelAction *curr = this->current_action;
	current_action = NULL;
	if (!curr) {
		DEBUG("trying to push NULL action...\n");
		return;
	}

	nextThread = advance_backtracking_state();
	curr->set_node(currentNode);
	set_backtracking(curr);
	currentNode = currentNode->explore_child(curr);
	this->action_trace->push_back(curr);
}

void ModelChecker::print_summary(void)
{
	printf("\n");
	printf("Number of executions: %d\n", num_executions);
	printf("Total nodes created: %d\n", TreeNode::getTotalNodes());

	scheduler->print();

	print_list(action_trace);
	printf("\n");

}

void ModelChecker::print_list(action_list_t *list)
{
	action_list_t::iterator it;

	printf("---------------------------------------------------------------------\n");
	printf("Trace:\n");

	for (it = list->begin(); it != list->end(); it++) {
		(*it)->print();
	}
	printf("---------------------------------------------------------------------\n");
}

int ModelChecker::add_thread(Thread *t)
{
	thread_map[id_to_int(t->get_id())] = t;
	scheduler->add_thread(t);
	return 0;
}

void ModelChecker::remove_thread(Thread *t)
{
	scheduler->remove_thread(t);
}

int ModelChecker::switch_to_master(ModelAction *act)
{
	Thread *old;

	DBG();
	old = thread_current();
	set_current_action(act);
	old->set_state(THREAD_READY);
	return Thread::swap(old, get_system_context());
}
