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
        // Don't instrument our own runtime — infinite recursion otherwise
        if (F.getName().contains("logAccess") ||
            F.getName().contains("analyzeAndPrint"))
            return PreservedAnalyses::all();

        Module* M = F.getParent();
        LLVMContext& Ctx = F.getContext();
        const DataLayout& DL = M->getDataLayout();

        // Declare logAccess in this module so we can call it
        // void logAccess(void* address, size_t size, int type)
        FunctionCallee LogFunc = M->getOrInsertFunction(
            "logAccess",
            Type::getVoidTy(Ctx),           // return type: void
            PointerType::get(Ctx, 0),        // arg 0: void* (i8* in IR)
            Type::getInt64Ty(Ctx),          // arg 1: size_t
            Type::getInt32Ty(Ctx)           // arg 2: int (0=READ, 1=WRITE)
        );

        bool modified = false;

        for (auto& BB : F) {
            // errs() << "  Instruction: " << I.getOpcodeName() << "\n";

            // Phase 1: collect all loads and stores
            std::vector<Instruction*> toInstrument;
            for (auto& I : BB) {
                if (isa<LoadInst>(&I) || isa<StoreInst>(&I))
                    toInstrument.push_back(&I);
            }

            // Phase 2: instrument them
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