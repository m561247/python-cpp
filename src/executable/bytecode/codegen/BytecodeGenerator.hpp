#pragma once

#include "VariablesResolver.hpp"
#include "ast/AST.hpp"
#include "ast/optimizers/ConstantFolding.hpp"
#include "executable/FunctionBlock.hpp"
#include "executable/Label.hpp"
#include "executable/bytecode/instructions/Instructions.hpp"

#include "forward.hpp"
#include "utilities.hpp"

namespace codegen {

class BytecodeGenerator;

struct FunctionInfo
{
	size_t function_id;
	FunctionBlock &function;
	BytecodeGenerator *generator;

	FunctionInfo(size_t, FunctionBlock &, BytecodeGenerator *);
};

class BytecodeValue : public ast::Value
{
	Register m_register;

  public:
	BytecodeValue(const std::string &name, Register register_)
		: ast::Value(name), m_register(register_)
	{}

	Register get_register() const { return m_register; }

	virtual bool is_function() const { return false; }
};

class BytecodeStaticValue : public ast::Value
{
	size_t m_index;

  public:
	BytecodeStaticValue(size_t index) : ast::Value("constant"), m_index(index) {}

	size_t get_index() const { return m_index; }

	virtual bool is_function() const { return false; }
};

class BytecodeStackValue : public ast::Value
{
	Register m_stack_index;

  public:
	BytecodeStackValue(const std::string &name, Register stack_index)
		: ast::Value(name), m_stack_index(stack_index)
	{}

	Register get_stack_index() const { return m_stack_index; }

	virtual bool is_function() const { return false; }
};

class BytecodeFreeValue : public ast::Value
{
	Register m_free_var_index;

  public:
	BytecodeFreeValue(const std::string &name, Register free_var_index)
		: ast::Value(name), m_free_var_index(free_var_index)
	{}

	Register get_free_var_index() const { return m_free_var_index; }

	virtual bool is_function() const { return false; }
};

class BytecodeFunctionValue : public BytecodeValue
{
	FunctionInfo m_info;

  public:
	BytecodeFunctionValue(const std::string &name, Register register_, FunctionInfo &&info)
		: BytecodeValue(name, register_), m_info(std::move(info))
	{}

	const FunctionInfo &function_info() const { return m_info; }

	bool is_function() const final { return true; }
};

class BytecodeGenerator : public ast::CodeGenerator
{
	friend FunctionInfo;

	class ASTContext
	{
		std::stack<std::shared_ptr<ast::Arguments>> m_local_args;
		std::vector<const ast::ASTNode *> m_parent_nodes;

	  public:
		void push_local_args(std::shared_ptr<ast::Arguments> args)
		{
			m_local_args.push(std::move(args));
		}
		void pop_local_args() { m_local_args.pop(); }

		bool has_local_args() const { return !m_local_args.empty(); }

		void push_node(const ast::ASTNode *node) { m_parent_nodes.push_back(node); }
		void pop_node() { m_parent_nodes.pop_back(); }

		const std::shared_ptr<ast::Arguments> &local_args() const { return m_local_args.top(); }
		const std::vector<const ast::ASTNode *> &parent_nodes() const { return m_parent_nodes; }
	};

	struct Scope
	{
		std::string name;
		std::string mangled_name;

		std::unordered_map<std::string,
			std::variant<BytecodeValue *, BytecodeStackValue *, BytecodeFreeValue *>>
			locals;
	};

  public:
	static constexpr size_t start_register = 1;
	static constexpr size_t start_stack_index = 0;

  private:
	FunctionBlocks m_functions;
	std::unordered_map<std::string, std::reference_wrapper<FunctionBlocks::FunctionType>>
		m_function_map;

	std::unordered_map<std::string, std::unique_ptr<VariablesResolver::Scope>>
		m_variable_visibility;

	size_t m_function_id{ 0 };
	InstructionBlock *m_current_block{ nullptr };

	// a non-owning list of all generated Labels
	std::vector<Label *> m_labels;

	std::vector<size_t> m_frame_register_count;
	std::vector<size_t> m_frame_stack_value_count;
	std::vector<size_t> m_frame_free_var_count;

	std::unordered_map<std::string, std::reference_wrapper<size_t>> m_function_free_var_count;

	size_t m_value_index{ 0 };
	ASTContext m_ctx;
	std::stack<Scope> m_stack;

  public:
	static std::unique_ptr<Program> compile(std::shared_ptr<ast::ASTNode> node,
		std::vector<std::string> argv,
		compiler::OptimizationLevel lvl);

  private:
	BytecodeGenerator();
	~BytecodeGenerator();

	template<typename OpType, typename... Args> void emit(Args &&... args)
	{
		ASSERT(m_current_block)
		m_current_block->push_back(std::make_unique<OpType>(std::forward<Args>(args)...));
	}

	friend std::ostream &operator<<(std::ostream &os, BytecodeGenerator &generator);

	std::string to_string() const;

	const FunctionBlocks &functions() const { return m_functions; }

	const std::list<InstructionBlock> &function(size_t idx) const
	{
		ASSERT(idx < m_functions.functions.size())
		return std::next(m_functions.functions.begin(), idx)->blocks;
	}

	const InstructionBlock &function(size_t idx, size_t block) const
	{
		ASSERT(idx < m_functions.functions.size())
		auto f = std::next(m_functions.functions.begin(), idx);
		ASSERT(block < f->blocks.size())
		return *std::next(f->blocks.begin(), block);
	}

	std::shared_ptr<Label> make_label(const std::string &name, size_t function_id)
	{
		spdlog::debug("New label to be added: name={} function_id={}", name, function_id);
		auto new_label = std::make_shared<Label>(name, function_id);

		ASSERT(std::find(m_labels.begin(), m_labels.end(), new_label.get()) == m_labels.end())

		m_labels.emplace_back(new_label.get());

		return new_label;
	}

	const Label &label(const Label &l) const
	{
		if (auto it = std::find(m_labels.begin(), m_labels.end(), &l); it != m_labels.end()) {
			return **it;
		} else {
			ASSERT_NOT_REACHED()
		}
	}

	const std::vector<Label *> &labels() const { return m_labels; }

	void bind(Label &label)
	{
		ASSERT(std::find(m_labels.begin(), m_labels.end(), &label) != m_labels.end())
		auto &blocks = function(label.function_id());
		const auto instructions_size = std::transform_reduce(
			blocks.begin(), blocks.end(), 0u, std::plus<size_t>{}, [](const auto &ins) {
				return ins.size();
			});
		const size_t current_instruction_position = instructions_size;
		label.set_position(current_instruction_position);
	}

	size_t register_count() const { return m_frame_register_count.back(); }
	size_t stack_variable_count() const { return m_frame_stack_value_count.back(); }
	size_t free_variable_count() const { return m_frame_free_var_count.back(); }

	Register allocate_register()
	{
		spdlog::debug("New register: {}", m_frame_register_count.back());
		return m_frame_register_count.back()++;
	}

	Register allocate_stack_value()
	{
		spdlog::debug("New stack value: {}", m_frame_stack_value_count.back());
		return m_frame_stack_value_count.back()++;
	}

	Register allocate_free_value()
	{
		spdlog::debug("New free value: {}", m_frame_free_var_count.back());
		return m_frame_free_var_count.back()++;
	}

	BytecodeFunctionValue *create_function(const std::string &);

	InstructionBlock *allocate_block(size_t);

	void enter_function()
	{
		m_frame_register_count.emplace_back(start_register);
		m_frame_stack_value_count.emplace_back(start_stack_index);
		m_frame_free_var_count.emplace_back(start_stack_index);
	}

	void exit_function(size_t function_id);

	void store_name(const std::string &, BytecodeValue *);
	BytecodeValue *load_name(const std::string &);

	BytecodeStaticValue *load_const(const py::Value &, size_t);

	std::tuple<size_t, size_t> move_to_stack(const std::vector<Register> &args);
	BytecodeValue *build_dict(const std::vector<Register> &, const std::vector<Register> &);
	BytecodeValue *build_list(const std::vector<Register> &);
	BytecodeValue *build_tuple(const std::vector<Register> &);
	void emit_call(Register func, const std::vector<Register> &);
	void make_function(Register,
		const std::string &,
		const std::vector<Register> &,
		const std::vector<Register> &,
		const std::optional<Register> &);

  private:
#define __AST_NODE_TYPE(NodeType) ast::Value *visit(const ast::NodeType *node) override;
	AST_NODE_TYPES
#undef __AST_NODE_TYPE

	BytecodeValue *generate(const ast::ASTNode *node, size_t function_id)
	{
		m_ctx.push_node(node);
		const auto old_function_id = m_function_id;
		m_function_id = function_id;
		auto *value = node->codegen(this);
		m_function_id = old_function_id;
		m_ctx.pop_node();
		return static_cast<BytecodeValue *>(value);
	}

	BytecodeValue *create_value(const std::string &name)
	{
		m_values.push_back(std::make_unique<BytecodeValue>(
			name + std::to_string(m_value_index++), allocate_register()));
		return static_cast<BytecodeValue *>(m_values.back().get());
	}

	BytecodeValue *create_return_value()
	{
		m_values.push_back(
			std::make_unique<BytecodeValue>("%" + std::to_string(m_value_index++), 0));
		return static_cast<BytecodeValue *>(m_values.back().get());
	}

	BytecodeValue *create_value()
	{
		m_values.push_back(std::make_unique<BytecodeValue>(
			"%" + std::to_string(m_value_index++), allocate_register()));
		return static_cast<BytecodeValue *>(m_values.back().get());
	}

	BytecodeStackValue *create_stack_value()
	{
		m_values.push_back(std::make_unique<BytecodeStackValue>(
			"%" + std::to_string(m_value_index++), allocate_stack_value()));
		return static_cast<BytecodeStackValue *>(m_values.back().get());
	}

	BytecodeFreeValue *create_free_value()
	{
		m_values.push_back(std::make_unique<BytecodeFreeValue>(
			"%" + std::to_string(m_value_index++), allocate_free_value()));
		return static_cast<BytecodeFreeValue *>(m_values.back().get());
	}

	std::unique_ptr<Program> generate_executable(std::string, std::vector<std::string>);
	void relocate_labels(const FunctionBlocks &functions);

	void set_insert_point(InstructionBlock *block) { m_current_block = block; }

	std::string mangle_namespace(std::stack<BytecodeGenerator::Scope> s) const;

	void create_nested_scope(const std::string &name, const std::string &mangled_name);
};
}// namespace codegen