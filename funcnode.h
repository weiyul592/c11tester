#ifndef __FUNCNODE_H__
#define __FUNCNODE_H__

#include "hashset.h"
#include "classlist.h"
#include "threads-model.h"

#define MAX_DIST 10
typedef ModelList<FuncInst *> func_inst_list_mt;

typedef enum edge_type {
	IN_EDGE, OUT_EDGE, BI_EDGE
} edge_type_t;

class FuncNode {
public:
	FuncNode(ModelHistory * history);
	~FuncNode();

	uint32_t get_func_id() { return func_id; }
	const char * get_func_name() { return func_name; }
	void set_func_id(uint32_t id) { func_id = id; }
	void set_func_name(const char * name) { func_name = name; }
	void set_new_exec_flag();

	void add_inst(ModelAction *act);
	FuncInst * get_inst(ModelAction *act);

	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> * getFuncInstMap() { return &func_inst_map; }
	func_inst_list_mt * get_inst_list() { return &inst_list; }
	func_inst_list_mt * get_entry_insts() { return &entry_insts; }
	void add_entry_inst(FuncInst * inst);

	void update_tree(action_list_t * act_list);
	void update_inst_tree(func_inst_list_t * inst_list);
	void update_predicate_tree(action_list_t * act_list);
	bool follow_branch(Predicate ** curr_pred, FuncInst * next_inst, ModelAction * next_act, SnapVector<Predicate *> * unset_predicates);

	void incr_exit_count() { exit_count++; }
	uint32_t get_exit_count() { return exit_count; }

	SnapList<action_list_t *> * get_action_list_buffer() { return action_list_buffer; }

	void add_to_val_loc_map(uint64_t val, void * loc);
	void add_to_val_loc_map(value_set_t * values, void * loc);
	void update_loc_may_equal_map(void * new_loc, loc_set_t * old_locations);

	void init_predicate_tree_position(thread_id_t tid);
	void set_predicate_tree_position(thread_id_t tid, Predicate * pred);
	Predicate * get_predicate_tree_position(thread_id_t tid);

	void init_inst_act_map(thread_id_t tid);
	void reset_inst_act_map(thread_id_t tid);
	void update_inst_act_map(thread_id_t tid, ModelAction * read_act);
	inst_act_map_t * get_inst_act_map(thread_id_t tid);

	void add_out_edge(FuncNode * other);
	ModelList<FuncNode *> * get_out_edges() { return &out_edges; }
	int compute_distance(FuncNode * target, int max_step = MAX_DIST);

	void print_predicate_tree();
	void print_val_loc_map();
	void print_last_read(thread_id_t tid);

	MEMALLOC
private:
	uint32_t func_id;
	const char * func_name;
	ModelHistory * history;
	Predicate * predicate_tree_entry;	// a dummy node whose children are the real entries

	uint32_t exit_count;
	uint32_t marker;

	void incr_marker() { marker++; }

	/* Use source line number as the key of hashtable, to check if
	 * atomic operation with this line number has been added or not
	 */
	HashTable<const char *, FuncInst *, uintptr_t, 4, model_malloc, model_calloc, model_free> func_inst_map;

	/* List of all atomic actions in this function */
	func_inst_list_mt inst_list;

	/* Possible entry atomic actions in this function */
	func_inst_list_mt entry_insts;

	void infer_predicates(FuncInst * next_inst, ModelAction * next_act, HashTable<void *, ModelAction *, uintptr_t, 0> * loc_act_map, SnapVector<struct half_pred_expr *> * half_pred_expressions);
	void generate_predicates(Predicate ** curr_pred, FuncInst * next_inst, SnapVector<struct half_pred_expr *> * half_pred_expressions);
	bool amend_predicate_expr(Predicate ** curr_pred, FuncInst * next_inst, ModelAction * next_act);

	/* Store action_lists when calls to update_tree are deferred */
	SnapList<action_list_t *> * action_list_buffer;

	/* Set of locations read by this FuncNode */
	loc_set_t * read_locations;

	/* Set of locations written to by this FuncNode */
	loc_set_t * write_locations;

	/* Keeps track of locations that have the same values written to */
	HashTable<uint64_t, loc_set_t *, uint64_t, 0> * val_loc_map;

	/* Keeps track of locations that may share the same value as key, deduced from val_loc_map */
	HashTable<void *, loc_set_t *, uintptr_t, 0> * loc_may_equal_map;

	// value_set_t * values_may_read_from;

	/* Run-time position in the predicate tree for each thread */
	ModelVector<Predicate *> predicate_tree_position;

	/* Store the relation between this FuncNode and other FuncNodes */
	HashTable<FuncNode *, edge_type_t, uintptr_t, 0, model_malloc, model_calloc, model_free> edge_table;

	/* FuncNodes that follow this node */
	ModelList<FuncNode *> out_edges;
};

#endif /* __FUNCNODE_H__ */
