#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

struct MemTracePass : public PassInfoMixin<MemTracePass>
{
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM)
    {
        errs() << "MemTracePass running on function: " << F.getName() << "\n";

        if (F.getName().contains("logAccess") ||
            F.getName().contains("analyzeAndPrint"))
            return PreservedAnalyses::all();

        Module* M = F.getParent();
        LLVMContext& Ctx = F.getContext();
        const DataLayout& DL = M->getDataLayout();



        FunctionCallee LogFunc = M->getOrInsertFunction(
            "logAccess",
            Type::getVoidTy(Ctx),
            PointerType::get(Ctx, 0),
            Type::getInt64Ty(Ctx),
            Type::getInt32Ty(Ctx)
        );


        FunctionCallee AnalyzeFunc = M->getOrInsertFunction(
            "analyzeAndPrint",
            Type::getVoidTy(Ctx)
        );

        bool modified = false;

        for (auto& BB : F) {

            std::vector<Instruction*> toInstrument;
            for (auto& I : BB) {
                if (isa<LoadInst>(&I) || isa<StoreInst>(&I))
                    toInstrument.push_back(&I);
            }


            for (auto* I : toInstrument) {
                Value* ptr = nullptr;
                uint64_t size = 0;
                int accessType = -1;

                if (auto* load = dyn_cast<LoadInst>(I)) {
                    ptr = load->getPointerOperand();
                    size = DL.getTypeStoreSize(load->getType());
                    accessType = 0;
                }
                else if (auto* store = dyn_cast<StoreInst>(I)) {
                    ptr = store->getPointerOperand();
                    size = DL.getTypeStoreSize(store->getValueOperand()->getType());
                    accessType = 1;
                }


                if (!ptr) continue;

                IRBuilder<> builder(I);
                Value* castPtr = ptr;

                builder.CreateCall(LogFunc, {
                    castPtr,
                    ConstantInt::get(Type::getInt64Ty(Ctx), size),
                    ConstantInt::get(Type::getInt32Ty(Ctx), accessType)
                });

                modified = true;
            }


            if (F.getName() == "main") {

                std::vector<ReturnInst*> returns;
                for (auto& I : BB) {
                    if (auto* ret = dyn_cast<ReturnInst>(&I)) {
                        returns.push_back(ret);
                    }
                }


                for (auto* ret : returns) {
                    IRBuilder<> builder(ret);
                    builder.CreateCall(AnalyzeFunc);
                    modified = true;
                }
            }
        }

        return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
    return {
        LLVM_PLUGIN_API_VERSION,
        "MemTracePass",
        "v0.1",
        [](PassBuilder& PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager& FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "memtrace") {
                        FPM.addPass(MemTracePass());
                        return true;
                    }
                    return false;
                });
        }
    };
}
