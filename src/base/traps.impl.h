#pragma once

#include <base/traps.h>
#include <base/functions.impl.h>
#include <base/logging.h>
#include <base/debugger.h>

#include <cassert>
#include <iostream>
#include <functional>

namespace Executor
{
namespace builtinlibs
{
    void addPPCEntrypoint(const char *library, const char *function, std::function<uint32_t (PowerCore&)> code);
}

namespace traps
{

namespace selectors
{
template <uint32_t mask>
struct D0
{
    static const uint32_t selectorMask = mask;
    static uint32_t get() { return EM_D0 & mask; }
};
template <uint32_t mask>
struct D1
{
    static const uint32_t selectorMask = mask;
    static uint32_t get() { return EM_D1 & mask; }
};


template <uint32_t mask>
struct StackWMasked
{
    static const uint32_t selectorMask = mask;
    static uint32_t get()
    {
        auto ret = POPADDR();
        auto sel = POPUW();
        PUSHADDR(ret);
        return sel & mask;
    }
};

template <uint32_t mask>
struct StackLMasked
{
    static const uint32_t selectorMask = mask;
    static uint32_t get()
    {
        auto ret = POPADDR();
        auto sel = POPUL();
        PUSHADDR(ret);
        return sel & mask;
    }
};
template <uint32_t mask>
struct StackWLookahead
{
    static const uint32_t selectorMask = mask;
    static uint32_t get()
    {
        return READUW(EM_A7 + 4) & mask;
    }
};

} /* end namespace selectors */

template<typename F>
syn68k_addr_t callback_install (const F& func)
{
    return ::callback_install(
        [](syn68k_addr_t a, void * b) -> syn68k_addr_t
        {
            currentCPUMode = CPUMode::m68k;
            currentM68KPC = *ptr_from_longint<GUEST<uint32>*>(EM_A7);
            const F& f = *(const F*)b;
            return f(a);
        },
        (void*)new F(func)
    );
}

template<typename Ret, typename... Args, Ret (*fptr)(Args...), typename CallConv>
void WrappedFunction<Ret (Args...), fptr, CallConv>::init()
{
    Entrypoint::init();
    if(logging::enabled())
        guestFP = (UPP<Ret (Args...),CallConv>)SYN68K_TO_US(callback_install(
                [this](syn68k_addr_t addr)
                {
                    if(breakpoint && base::Debugger::instance)
                        if(auto ret = base::Debugger::instance->trapBreak68K(addr, name); ret != (uint32_t)~0)
                            return ret;

                    return callfrom68K::Invoker<Ret (Args...), CallConv>
                        ::invokeFrom68K(addr, logging::makeLoggedFunction<CallConv>(name, fptr));
                }
            ));    
    else
        guestFP = (UPP<Ret (Args...),CallConv>)SYN68K_TO_US(callback_install(
                [this](syn68k_addr_t addr)
                {
                    if(breakpoint && base::Debugger::instance)
                        if(auto ret = base::Debugger::instance->trapBreak68K(addr, name); ret != (uint32_t)~0)
                            return ret;

                    return callfrom68K::Invoker<Ret (Args...), CallConv>
                        ::invokeFrom68K(addr, fptr);
                }
            ));    

    if(libname)
    {
        if(logging::enabled())
            builtinlibs::addPPCEntrypoint(libname, name,
                [this](PowerCore& cpu) { return callfromPPC::Invoker<Ret (Args...)>::invokeFromPPC(cpu, logging::makeLoggedFunction(name, fptr)); }
            );
        else
            builtinlibs::addPPCEntrypoint(libname, name,
                [](PowerCore& cpu) { return callfromPPC::Invoker<Ret (Args...)>::invokeFromPPC(cpu, fptr); }
            );
    }
}

template<typename Ret, typename... Args, Ret (*fptr)(Args...), int trapno, typename CallConv>
Ret TrapFunction<Ret (Args...), fptr, trapno, CallConv>::invokeViaTrapTable(Args... args) const
{
    return (UPP<Ret (Args...),CallConv>(SYN68K_TO_US(tableEntry())))(args...);
}

template<typename Ret, typename... Args, Ret (*fptr)(Args...), int trapno, typename CallConv>
void TrapFunction<Ret (Args...), fptr, trapno, CallConv>::init()
{
    WrappedFunction<Ret (Args...), fptr, CallConv>::init();
    originalFunction = US_TO_SYN68K(((void*)this->guestFP));
    assert(trapno);
    assert(!tableEntry());
    tableEntry() = originalFunction;
}

template<typename Ret, typename... Args, Ret (*fptr)(Args...), int trapno, uint32_t selector, typename CallConv>
SubTrapFunction<Ret (Args...), fptr, trapno, selector, CallConv>::SubTrapFunction(
    const char* name, GenericDispatcherTrap& dispatcher, const char *exportToLib)
    : WrappedFunction<Ret(Args...),fptr,CallConv>(name, exportToLib), dispatcher(dispatcher)
{
}

template<typename Ret, typename... Args, Ret (*fptr)(Args...), int trapno, uint32_t selector, typename CallConv>
void SubTrapFunction<Ret (Args...), fptr, trapno, selector, CallConv>::init()
{
    WrappedFunction<Ret(Args...),fptr,CallConv>::init();
    if(logging::enabled())
        dispatcher.addSelector(selector,
            [this](syn68k_addr_t addr)
            {
                if(this->breakpoint && base::Debugger::instance)
                    if(auto ret = base::Debugger::instance->trapBreak68K(addr, this->name); ret != (uint32_t)~0)
                        return ret;

                return callfrom68K::Invoker<Ret (Args...), CallConv>
                    ::invokeFrom68K(addr, logging::makeLoggedFunction<CallConv>(this->name, fptr));
            }
        );
    else
        dispatcher.addSelector(selector,
            [this](syn68k_addr_t addr)
            {
                if(this->breakpoint && base::Debugger::instance)
                    if(auto ret = base::Debugger::instance->trapBreak68K(addr, this->name); ret != (uint32_t)~0)
                        return ret;

                return callfrom68K::Invoker<Ret (Args...), CallConv>
                    ::invokeFrom68K(addr, fptr); 
            }
        );
}

template<class SelectorConvention>
syn68k_addr_t DispatcherTrap<SelectorConvention>::invokeFrom68K(syn68k_addr_t addr, void* extra)
{
    DispatcherTrap<SelectorConvention>* self = (DispatcherTrap<SelectorConvention>*)extra;

    if(self->breakpoint && base::Debugger::instance)
        if(auto ret = base::Debugger::instance->trapBreak68K(addr, self->name); ret != (uint32_t)~0)
            return ret;

    uint32 sel = SelectorConvention::get();
    auto it = self->selectors.find(sel);
    if(it != self->selectors.end())
        return it->second(addr);
    else
    {
        std::cerr << "Unknown selector 0x" << std::hex << sel << " for trap " << self->name << std::endl;
        std::abort();
    }
}

template<class SelectorConvention>
void DispatcherTrap<SelectorConvention>::addSelector(uint32_t sel, std::function<syn68k_addr_t(syn68k_addr_t)> handler)
{
    selectors[sel & SelectorConvention::selectorMask] = handler;
}

template<class SelectorConvention>
void DispatcherTrap<SelectorConvention>::init()
{
    GenericDispatcherTrap::init();
    if(trapno)
    {
        ProcPtr guestFP = (ProcPtr)SYN68K_TO_US(::callback_install(&invokeFrom68K, this));
        if(trapno & TOOLBIT)
        {
            tooltraptable[trapno & 0x3FF] = US_TO_SYN68K(((void*)guestFP));
        }
        else
        {
            ostraptable[trapno & 0xFF] = US_TO_SYN68K(((void*)guestFP));
        }
    }
}

template<typename Trap, typename Ret, typename... Args, bool... flags>
void TrapVariant<Trap, Ret (Args...), flags...>::init()
{
    Entrypoint::init();
    if(libname)
    {
        if(logging::enabled())
        {
            builtinlibs::addPPCEntrypoint(libname, name,
                [this](PowerCore& cpu)
                {
                    return callfromPPC::Invoker<Ret (Args...)>::invokeFromPPC(cpu,
                        logging::makeLoggedFunction1<Ret (Args...)>(name, 
                            [this](Args... args) -> Ret { return (*this)(args...); }
                        )
                    );
                });
        }
        else
        {
            builtinlibs::addPPCEntrypoint(libname, name,
                [this](PowerCore& cpu)
                {
                    return callfromPPC::Invoker<Ret (Args...)>::invokeFromPPC(cpu,
                        [this](Args... args) -> Ret { return (*this)(args...); }
                    );
                });
        }
    }
}

template<typename Trap, typename Ret, typename... Args, bool... flags>
TrapVariant<Trap, Ret (Args...), flags...>::TrapVariant(const Trap& trap, const char* name, const char* exportToLib)
    : Entrypoint(name, exportToLib), trap(trap)
{
}


}
}
