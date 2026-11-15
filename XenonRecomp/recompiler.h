#pragma once

#include "pch.h"
#include "recompiler_config.h"

struct RecompilerLocalVariables
{
    bool ctr{};
    bool xer{};
    bool reserved{};
    bool cr[8]{};
    bool r[32]{};
    bool f[32]{};
    bool v[128]{};
    bool env{};
    bool temp{};
    bool vTemp{};
    bool ea{};
};

enum class CSRState
{
    Unknown,
    FPU,
    VMX
};

struct Recompiler
{
    // Enforce In-order Execution of I/O constant for quick comparison
    static constexpr uint32_t c_eieio = 0xAC06007C;
    Image image;
    std::vector<Function> functions;
    std::string out;
    size_t cppFileIndex = 0;
    RecompilerConfig config;

    // [Switch] Code generation (all three off with XENON_RECOMP_CLASSIC=1, for A/B builds):
    //  - loopBarriers: PPC_LOOP_BARRIER() on every backward branch, so that guest memory accesses
    //    can be plain ones (ppc_context.h) and loops still read memory again on every pass;
    //  - skipCalleeSaves: with non-volatile registers as locals, the prologue stores of callee-saved
    //    registers (and of the link register) write locals that nothing reads: they are dropped;
    //  - csrAcrossLabels: the FPSCR flush mode is known at a label when every way into it agrees,
    //    instead of being checked again after every label.
    bool loopBarriers = true;
    bool skipCalleeSaves = true;
    bool csrAcrossLabels = true;

    // Per function: the flush mode assumed at each label while printing, and the meet of the modes
    // recorded on the branches into each label (a missing entry means no branch recorded yet).
    std::unordered_map<uint32_t, CSRState> csrLabelIn;
    std::unordered_map<uint32_t, CSRState> csrLabelEdges;
    void RecordCsrEdge(uint32_t target, CSRState state);

    // Addresses of the prologue stores dropped by skipCalleeSaves in the current function.
    std::unordered_set<uint32_t> skippedSaves;

    // No warnings while a function body is printed only to analyse it.
    bool quiet = false;

    bool LoadConfig(const std::string_view& configFilePath);

    template<class... Args>
    void print(fmt::format_string<Args...> fmt, Args&&... args)
    {
        fmt::vformat_to(std::back_inserter(out), fmt.get(), fmt::make_format_args(args...));
    }

    template<class... Args>
    void println(fmt::format_string<Args...> fmt, Args&&... args)
    {
        fmt::vformat_to(std::back_inserter(out), fmt.get(), fmt::make_format_args(args...));
        out += '\n';
    }

    void Analyse();

    // TODO: make a RecompileArgs struct instead this is getting messy
    bool Recompile(
        const Function& fn,
        uint32_t base,
        const ppc_insn& insn,
        const uint32_t* data,
        std::unordered_map<uint32_t, RecompilerSwitchTable>::iterator& switchTable,
        RecompilerLocalVariables& localVariables,
        CSRState& csrState);

    bool Recompile(const Function& fn);

    void Recompile(const std::filesystem::path& headerFilePath);

    void SaveCurrentOutData(const std::string_view& name = std::string_view());
};
