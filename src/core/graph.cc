#include "core/graph.h"
#include <algorithm>
#include <numeric>
#include <queue>
#include "operators/matmul.h"
#include "operators/transpose.h"

#include "core/ref.h"

namespace infini
{

    void GraphObj::addOperatorAndConnect(const Operator &op)
    {
        sorted = false;
        ops.push_back(op);
        for (auto &input : op->getInputs())
        {
            if (input)
            {
                input->addTarget(op);
                if (auto pred = input->getSource())
                {
                    pred->addSuccessors(op);
                    op->addPredecessors(pred);
                }
            }
        }
        for (auto &output : op->getOutputs())
        {
            if (output)
            {
                output->setSource(op);
                for (auto &succ : output->getTargets())
                {
                    succ->addPredecessors(op);
                    op->addSuccessors(succ);
                }
            }
        }
    }

    string GraphObj::toString() const
    {
        std::ostringstream oss;
        oss << "Graph Tensors:\n";
        for (const auto &tensor : tensors)
            oss << tensor << "\n";

        oss << "Graph operators:\n";
        for (const auto &op : ops)
        {
            vector<UidBaseType> preds, succs;
            for (auto &o : op->getPredecessors())
                preds.emplace_back(o->getGuid());
            for (auto &o : op->getSuccessors())
                succs.emplace_back(o->getGuid());
            oss << "OP " << op->getGuid();
            oss << ", pred " << vecToString(preds);
            oss << ", succ " << vecToString(succs);
            oss << ", " << op << "\n";
        }
        return oss.str();
    }

    bool GraphObj::topo_sort()
    {
        if (this->sorted)
        {
            return true;
        }
        std::vector<Operator> sorted;
        std::unordered_set<OperatorObj *> flags;
        sorted.reserve(ops.size());
        flags.reserve(ops.size());
        while (sorted.size() < ops.size())
        {
            // Any node is move to sorted in this loop.
            auto modified = false;
            for (auto const &op : ops)
            {
                if (auto const &inputs = op->getInputs();
                    flags.find(op.get()) == flags.end() &&
                    std::all_of(inputs.begin(), inputs.end(),
                                [&flags](auto const &input)
                                {
                                    auto ptr = input->getSource().get();
                                    return !ptr || flags.find(ptr) != flags.end();
                                }))
                {
                    modified = true;
                    sorted.emplace_back(op);
                    flags.insert(op.get());
                }
            }
            if (!modified)
            {
                return false;
            }
        }
        this->ops = std::move(sorted);
        return this->sorted = true;
    }

    void GraphObj::optimize()
    {
        // =================================== 作业 ===================================
        // TODO: 设计一个算法来实现指定的图优化规则
        // 图优化规则如下：
        // 1. 去除冗余的算子（例如，两个相邻的算子都是 transpose 算子，且做的是相反的操作，可以将其全部删除）
        // 2. 合并算子（例如，矩阵乘算子中含有属性transA、transB，如果其输入存在transpose，且对最后两个维度做交换，就可以将transpose融入到矩阵乘算子的属性中去）
        // =================================== 作业 ===================================
        //优化图结构,合并操作(1.删除冗余算子，2.合并算子)
        if(ops.size()==0) return;//无算子，返回
        topo_sort();//确保被排序
        //1
        OpVec need_to_remove_operators;
        for(auto op: ops){
            switch(op->getOpType().type){
                case OpType::Transpose:
                    {
                        auto input = op->getInputs()[0];
                        auto output = op->getOutput();
                        if (output->getTargets().size() == 1) // only one target
                        {
                            auto next_op = output->getTargets()[0];
                            if (next_op->getOpType() == OpType::Transpose)
                            {
                                auto next_output = next_op->getOutput();
                                if (next_output->getDims() == input->getDims())
                                {
                                    need_to_remove_operators.push_back(op);
                                    need_to_remove_operators.push_back(next_op);
                                    for (auto target : next_output->getTargets())
                                    {
                                        input->addTarget(target);
                                        target->replaceInput(next_output, input);
                                        target->removePredecessors(next_op);
                                    }
                                    input->removeTarget(op);
                                    removeTensor(output);
                                    removeTensor(next_output);
                                }
                            }
                        }
                        break;

                    }
                    
                case OpType::MatMul:
                    {   
                        auto matlut_op = as<MatmulObj>(op);
                        auto input_a = op->getInputs()[0];
                        auto input_b = op->getInputs()[1];
                        auto output = op->getOutput();
                        if (input_a->getSource() && input_a->getSource()->getOpType() == OpType::Transpose)
                        {
                            auto transpose_op = input_a->getSource();
                            if (as<TransposeObj>(transpose_op)->getPermute() == Shape{0, 1, 3, 2})
                            {
                                if(transpose_op->getOutput()->getTargets().size() == 1){
                                    matlut_op->setTransB(true);
                                    auto transpose_input = transpose_op->getInputs()[0];
                                    transpose_input->removeTarget(transpose_op);
                                    transpose_input->addTarget(matlut_op);
                                    matlut_op->replaceInput(input_a, transpose_input);
                                    matlut_op->removePredecessors(transpose_op);
                                    need_to_remove_operators.push_back(transpose_op);
                                    removeTensor(transpose_op->getOutput());
                                }
                            }
                        }
                        if (input_b->getSource() && input_b->getSource()->getOpType() == OpType::Transpose)
                        {
                            auto transpose_op = input_b->getSource();
                            if (as<TransposeObj>(transpose_op)->getPermute() == Shape{0, 1, 3, 2})
                            {
                                if(transpose_op->getOutput()->getTargets().size() == 1){
                                    matlut_op->setTransB(true);
                                    auto transpose_input = transpose_op->getInputs()[0];
                                    transpose_input->removeTarget(transpose_op);
                                    transpose_input->addTarget(matlut_op);
                                    matlut_op->replaceInput(input_b, transpose_input);
                                    matlut_op->removePredecessors(transpose_op);
                                    need_to_remove_operators.push_back(transpose_op);
                                    removeTensor(transpose_op->getOutput());
                                }
                            }
                        }

                    }
                    break;
                default:
                    // 其余算子不做考虑
                    break;
            }
            
        }
        for (auto op : need_to_remove_operators)
        {
            removeOperator(op);
        }
        
    }

    Tensor GraphObj::getTensor(int fuid) const
    {
        for (auto tensor : tensors)
        {
            if (tensor->getFuid() == fuid)
            {
                return tensor;
            }
        }
        return nullptr;
    }

    void GraphObj::shape_infer()
    {
        for (auto &op : ops)
        {
            auto ans = op->inferShape();
            IT_ASSERT(ans.has_value());
            auto oldOutputs = op->getOutputs();
            IT_ASSERT(ans.value().size() == oldOutputs.size());
            // replace the old outputshape and size with new one
            for (int i = 0; i < (int)ans.value().size(); ++i)
            {
                auto newShape = ans.value()[i];
                auto oldShape = oldOutputs[i]->getDims();
                auto fuid = oldOutputs[i]->getFuid();
                if (newShape != oldShape)
                {
                    auto tensor = this->getTensor(fuid);
                    tensor->setShape(newShape);
                }
            }
        }
    }

    void GraphObj::dataMalloc()
    {
        // topological sorting first
        IT_ASSERT(topo_sort() == true);

        // =================================== 作业 ===================================
        // TODO：利用 allocator 给计算图分配内存
        // HINT: 获取分配好的内存指针后，可以调用 tensor 的 setDataBlob 函数给 tensor 绑定内存
        // =================================== 作业 ===================================
        std::unordered_map<UidBaseType, size_t> offsets;//为每一个tensor申请空间
        for (auto &tensor : tensors)
        {
            auto offset = allocator.alloc(tensor->getBytes());
            offsets[tensor->getFuid()] = offset;
        }
        auto base_addr = static_cast<char*>(allocator.getPtr());
        for (auto &tensor : tensors)
        {
            auto offset = offsets[tensor->getFuid()];
            tensor->setDataBlob(make_ref<BlobObj>(runtime, base_addr + offset));
        }
        allocator.info();
    }

    Tensor GraphObj::addTensor(Shape dim, DataType dtype)
    {
        return tensors.emplace_back(make_ref<TensorObj>(dim, dtype, runtime));
    }

    Tensor GraphObj::addTensor(const Tensor &tensor)
    {
        IT_ASSERT(tensor->getRuntime() == runtime,
                  std::string("Tensor runtime mismatch: cannot add a tenosr in ") +
                      tensor->getRuntime()->toString() + " to " +
                      runtime->toString());
        tensors.emplace_back(tensor);
        return tensor;
    }

    TensorVec GraphObj::addTensor(const TensorVec &tensors)
    {
        for (auto &t : tensors)
            addTensor(t);
        return tensors;
    }

    // tensor's "source" and "target" must be in "ops".
    // tensor has no "source" and no "target" must not exist.
    // "inputs" or "outputs" of operators must be in "tensors"
    // "predecessors" and "successors" of an operator of "ops" must be in "ops".
    bool GraphObj::checkValid() const
    {
        for (auto tensor : tensors)
        {
            IT_ASSERT(!(tensor->getTargets().size() == 0 &&
                        nullptr == tensor->getSource()));
            for (auto op : tensor->getTargets())
            {
                IT_ASSERT(std::find(ops.begin(), ops.end(), op) != ops.end());
            }
            auto op = tensor->getSource();
            IT_ASSERT(!(op && std::find(ops.begin(), ops.end(), op) == ops.end()));
        }
        for (auto op : ops)
        {
            for (auto tensor : op->getInputs())
            {
                IT_ASSERT(std::find(tensors.begin(), tensors.end(), tensor) !=
                          tensors.end());
            }
            for (auto tensor : op->getOutputs())
            {
                IT_ASSERT(std::find(tensors.begin(), tensors.end(), tensor) !=
                          tensors.end());
            }
            for (auto pre : op->getPredecessors())
            {
                IT_ASSERT(std::find(ops.begin(), ops.end(), pre) != ops.end());
            }
            for (auto suc : op->getSuccessors())
            {
                IT_ASSERT(std::find(ops.begin(), ops.end(), suc) != ops.end());
            }
        }
        std::set<UidBaseType> s;
        // check whether two tensors with the same FUID exist
        for (auto tensor : tensors)
        {
            int cnt = s.count(tensor->getFuid());
            IT_ASSERT(cnt == 0, std::to_string(tensor->getFuid()));
            s.insert(tensor->getFuid());
        }
        return true;
    }

} // namespace infini