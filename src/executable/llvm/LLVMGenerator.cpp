#include "LLVMGenerator.hpp"

#include "executable/Program.hpp"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace codegen;

namespace {
static AllocaInst *create_entry_block_alloca(llvm::Function *f, const std::string &name, Type *type)
{
	IRBuilder<> b(&f->getEntryBlock(), f->getEntryBlock().begin());
	return b.CreateAlloca(type, 0, name);
}
}// namespace

struct LLVMGenerator::Context
{
	struct State
	{
		enum class Status { OK, ERROR };
		Status status{ Status::OK };
	};

	struct Scope
	{
		std::map<std::string, llvm::AllocaInst *> lookup_table;
		Scope *parent{ nullptr };
	};

	State state;
	std::stack<Scope> scope_stack;
	std::unique_ptr<LLVMContext> ctx;
	std::unique_ptr<IRBuilder<>> builder;
	std::unique_ptr<Module> module;
	llvm::Value *last_value{ nullptr };

	Context(const std::string &module_name)
	{
		ctx = std::make_unique<LLVMContext>();
		builder = std::make_unique<IRBuilder<>>(*ctx);
		module = std::make_unique<Module>(module_name, *ctx);
	}

	// void add_global(StringRef name, llvm::Value* value) {
	// 	module->getOrInsertGlobal(name, value->)
	// }

	void push_stack() { scope_stack.push(Scope{ .parent = &scope_stack.top() }); }
	void pop_stack() { scope_stack.pop(); }

	void add_local(StringRef name, llvm::AllocaInst *value)
	{
		ASSERT(!scope_stack.empty())
		scope_stack.top().lookup_table[name] = value;
	}

	llvm::AllocaInst *get_variable(StringRef name)
	{
		ASSERT(!scope_stack.empty())
		// search in local scope
		if (auto it = scope_stack.top().lookup_table.find(name);
			it != scope_stack.top().lookup_table.end()) {
			return it->second;
		}

		return nullptr;
	}
};

LLVMGenerator::LLVMGenerator() : m_ctx(std::make_unique<Context>("test")) {}

std::shared_ptr<Program> LLVMGenerator::compile(std::shared_ptr<ast::ASTNode> node,
	std::vector<std::string> argv,
	compiler::OptimizationLevel lvl)
{
	auto module = as<ast::Module>(node);
	ASSERT(module)

	auto generator = LLVMGenerator();

	node->codegen(&generator);

	if (generator.m_ctx->state.status == LLVMGenerator::Context::State::Status::ERROR) {
		return nullptr;
	}

	std::vector<std::shared_ptr<::Function>> module_functions;
	for (const auto &f : generator.m_ctx->module->functions()) {
		module_functions.emplace_back(std::make_shared<LLVMFunction>(f));
	}

	return std::make_shared<Program>(std::move(module_functions), module->filename(), argv);
}

llvm::Value *LLVMGenerator::generate(const ast::ASTNode *node)
{
	if (m_ctx->state.status == LLVMGenerator::Context::State::Status::ERROR) { return nullptr; }
	node->codegen(this);
	return m_ctx->last_value;
}

void LLVMGenerator::visit(const ast::Argument *node) { TODO(); }

void LLVMGenerator::visit(const ast::Arguments *node) { TODO(); }

void LLVMGenerator::visit(const ast::Attribute *node) { TODO(); }

void LLVMGenerator::visit(const ast::Assign *node)
{
	auto *value_to_store = generate(node->value().get());
	for (const auto &target : node->targets()) {
		if (auto ast_name = as<ast::Name>(target)) {
			ASSERT(ast_name->ids().size() == 1)
			const auto &var_name = ast_name->ids()[0];
			llvm::Function *f = m_ctx->builder->GetInsertBlock()->getParent();
			Type *type = value_to_store->getType();
			if (auto it = m_ctx->scope_stack.top().lookup_table.find(var_name);
				it != m_ctx->scope_stack.top().lookup_table.end()) {
				if (type != it->second->getType()) {
					std::string repr1;
					raw_string_ostream out1{ repr1 };
					type->print(out1);

					std::string repr2;
					raw_string_ostream out2{ repr2 };
					type->print(out2);

					set_error_state("Cannot assign value of type '{}' to variable of type '{}",
						out1.str(),
						out2.str());
				}
			}
			auto *alloca_inst = create_entry_block_alloca(f, var_name, type);
			m_ctx->add_local(var_name, alloca_inst);
			m_ctx->builder->CreateStore(value_to_store, alloca_inst);
		} else {
			set_error_state("Can only assign to ast::Name");
			return;
		}
	}
}

void LLVMGenerator::visit(const ast::Assert *node) { TODO(); }

void LLVMGenerator::visit(const ast::AugAssign *node) { TODO(); }

void LLVMGenerator::visit(const ast::BinaryExpr *node)
{
	switch (node->op_type()) {
	case ast::BinaryOpType::PLUS: {
		auto *lhs = generate(node->lhs().get());
		if (!lhs) return;
		auto *rhs = generate(node->rhs().get());
		if (!rhs) return;
		m_ctx->last_value = m_ctx->builder->CreateAdd(lhs, rhs);
	} break;
	case ast::BinaryOpType::MINUS: {
		TODO();
	} break;
	case ast::BinaryOpType::MODULO: {
		TODO();
	} break;
	case ast::BinaryOpType::MULTIPLY: {
		TODO();
	} break;
	case ast::BinaryOpType::EXP: {
		TODO();
	} break;
	case ast::BinaryOpType::SLASH: {
		TODO();
	} break;
	case ast::BinaryOpType::FLOORDIV: {
		TODO();
	} break;
	case ast::BinaryOpType::LEFTSHIFT: {
		TODO();
	} break;
	case ast::BinaryOpType::RIGHTSHIFT: {
		TODO();
	} break;
	}
}

void LLVMGenerator::visit(const ast::BoolOp *node) { TODO(); }

void LLVMGenerator::visit(const ast::Call *node) { TODO(); }

void LLVMGenerator::visit(const ast::ClassDefinition *node) { TODO(); }

void LLVMGenerator::visit(const ast::Compare *node) { TODO(); }

void LLVMGenerator::visit(const ast::Constant *node) { TODO(); }

void LLVMGenerator::visit(const ast::Dict *node) { TODO(); }

void LLVMGenerator::visit(const ast::ExceptHandler *node) { TODO(); }

void LLVMGenerator::visit(const ast::For *node) { TODO(); }

llvm::Type *LLVMGenerator::arg_type(const std::shared_ptr<ast::ASTNode> &type_annotation)
{
	if (auto name = as<ast::Name>(type_annotation)) {
		if (name->ids().size() != 1) {
			set_error_state("arg type is not a AST node with one id, cannot determine arg type");
			return nullptr;
		}
		const auto &type_name = name->ids()[0];
		if (type_name == "int") {
			return Type::getInt64Ty(*m_ctx->ctx);
		} else if (type_name == "float") {
			return Type::getDoubleTy(*m_ctx->ctx);
		} else {
			set_error_state("arg type {} currenlty not supported", type_name);
			return nullptr;
		}
	} else {
		set_error_state("arg type is not a Name AST node, cannot determine arg type");
	}
	return nullptr;
}// namespace

void LLVMGenerator::visit(const ast::FunctionDefinition *node)
{
	std::vector<Type *> arg_types;

	for (const auto &arg : node->args()->args()) {
		const auto &type_annotation = arg->annotation();
		if (!type_annotation) {
			set_error_state("arg is not type annotated, cannot determine arg type");
			return;
		}
		if (auto *type = arg_type(type_annotation)) {
			arg_types.push_back(type);
		} else {
			return;
		}
	}

	ASSERT(arg_types.size() == node->args()->args().size())

	if (!node->returns()) {
		// TODO: this would require type checking through the whole function
		set_error_state("empty return type annotation not implemented");
		return;
	}

	const auto &return_type_annotation = node->returns();
	Type *return_type = nullptr;
	if (auto *type = arg_type(return_type_annotation)) {
		return_type = type;
	} else {
		return;
	}

	auto *FT = FunctionType::get(return_type, arg_types, false);
	auto *F = llvm::Function::Create(
		FT, llvm::Function::ExternalLinkage, node->name(), m_ctx->module.get());
	BasicBlock *BB = BasicBlock::Create(*m_ctx->ctx, "entry", F);
	m_ctx->builder->SetInsertPoint(BB);

	m_ctx->push_stack();

	for (size_t i = 0; const auto &arg : node->args()->args()) {
		auto *llvm_arg = F->getArg(i);
		llvm_arg->setName(arg->name());
		auto *allocation = create_entry_block_alloca(F, arg->name(), arg_types[i]);
		m_ctx->add_local(arg->name(), allocation);
		m_ctx->builder->CreateStore(llvm_arg, allocation);
		i++;
	}

	for (const auto &statement : node->body()) { generate(statement.get()); }

	m_ctx->pop_stack();
}

void LLVMGenerator::visit(const ast::If *node) { TODO(); }

void LLVMGenerator::visit(const ast::Import *node) { TODO(); }

void LLVMGenerator::visit(const ast::Keyword *node) { TODO(); }

void LLVMGenerator::visit(const ast::List *node) { TODO(); }

void LLVMGenerator::visit(const ast::Module *node)
{
	m_ctx->module->setSourceFileName(node->filename());
	m_ctx->module->setModuleIdentifier("test");

	for (const auto &statement : node->body()) {
		statement->codegen(this);
		if (m_ctx->state.status == LLVMGenerator::Context::State::Status::ERROR) { return; }
	}

	std::string repr;
	raw_string_ostream out{ repr };
	m_ctx->module->print(out, nullptr);
	spdlog::debug("LLVM module:\n{}", out.str());
}

void LLVMGenerator::visit(const ast::Name *node)
{
	const auto &var_name = node->ids()[0];
	ASSERT(m_ctx->builder->GetInsertBlock())
	auto *current_func = m_ctx->builder->GetInsertBlock()->getParent();
	ASSERT(current_func)
	auto *var_alloca = m_ctx->get_variable(var_name);

	if (!var_alloca) {
		set_error_state("Could not find '{}' in function", var_name);
		return;
	}

	m_ctx->last_value =
		m_ctx->builder->CreateLoad(var_alloca->getAllocatedType(), var_alloca, var_name);
}

void LLVMGenerator::visit(const ast::Pass *node) { TODO(); }

void LLVMGenerator::visit(const ast::Raise *node) { TODO(); }

void LLVMGenerator::visit(const ast::Return *node)
{
	auto *return_value = generate(node->value().get());
	m_ctx->builder->CreateRet(return_value);
	m_ctx->last_value = return_value;
}

void LLVMGenerator::visit(const ast::Subscript *node) { TODO(); }

void LLVMGenerator::visit(const ast::Try *node) { TODO(); }

void LLVMGenerator::visit(const ast::Tuple *node) { TODO(); }

void LLVMGenerator::visit(const ast::UnaryExpr *node) { TODO(); }

void LLVMGenerator::visit(const ast::While *node) { TODO(); }

template<typename... Args>
void LLVMGenerator::set_error_state(std::string_view msg, Args &&... args)
{
	spdlog::debug(msg, std::forward<Args>(args)...);
	m_ctx->state.status = Context::State::Status::ERROR;
}

LLVMFunction::LLVMFunction(const llvm::Function &f)
	: Function(0, f.getName().str(), FunctionExecutionBackend::LLVM), m_function(f)
{}

std::string LLVMFunction::to_string() const
{
	std::string repr;
	raw_string_ostream out{ repr };
	m_function.print(out, nullptr);
	return out.str();
}