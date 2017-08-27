/*******************************************************************************
 * Copyright (C) 2017, MINRES Technologies GmbH
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * Contributors:
 *       eyck@minres.com - initial API and implementation
 ******************************************************************************/

#ifndef _RISCV_CORE_H_
#define _RISCV_CORE_H_

#include <iss/vm_if.h>
#include <iss/arch_if.h>
#include <util/ities.h>
#include <util/sparse_array.h>
#include <elfio/elfio.hpp>
#include <easylogging++.h>
#include <sstream>

namespace iss {
namespace arch {

enum {
    tohost_dflt = 0xF0001000,
    fromhost_dflt = 0xF0001040
};

enum csr_name {
    /* user-level CSR */
    // User Trap Setup
    ustatus=0x000,
    uie=0x004,
    utvec=0x005,
    // User Trap Handling
    uscratch=0x040,
    uepc=0x041,
    ucause=0x042,
    utval=0x043,
    uip=0x044,
    // User Floating-Point CSRs
    fflags=0x001,
    frm=0x002,
    fcsr=0x003,
    // User Counter/Timers
    cycle=0xC00,
    time=0xC01,
    instret=0xC02,
    hpmcounter3=0xC03,
    hpmcounter4=0xC04,
    /*...*/
    hpmcounter31=0xC1F,
    cycleh=0xC80,
    timeh=0xC81,
    instreth=0xC82,
    hpmcounter3h=0xC83,
    hpmcounter4h=0xC84,
    /*...*/
    hpmcounter31h=0xC9F,
    /* supervisor-level CSR */
    // Supervisor Trap Setup
    sstatus=0x100,
    sedeleg=0x102,
    sideleg=0x103,
    sie=0x104,
    stvec=0x105,
    scounteren=0x106,
    // Supervisor Trap Handling
    sscratch=0x140,
    sepc=0x141,
    scause=0x142,
    stval=0x143,
    sip=0x144,
    // Supervisor Protection and Translation
    satp=0x180,
    /* machine-level CSR */
    // Machine Information Registers
    mvendorid=0xF11,
    marchid=0xF12,
    mimpid=0xF13,
    mhartid=0xF14,
    // Machine Trap Setup
    mstatus=0x300,
    misa=0x301,
    medeleg=0x302,
    mideleg=0x303,
    mie=0x304,
    mtvec=0x305,
    mcounteren=0x306,
    // Machine Trap Handling
    mscratch=0x340,
    mepc=0x341,
    mcause=0x342,
    mtval=0x343,
    mip=0x344,
    // Machine Protection and Translation
    pmpcfg0=0x3A0,
    pmpcfg1=0x3A1,
    pmpcfg2=0x3A2,
    pmpcfg3=0x3A3,
    pmpaddr0=0x3B0,
    pmpaddr1=0x3B1,
    /*...*/
    pmpaddr15=0x3BF,
    // Machine Counter/Timers
    mcycle=0xB00,
    minstret=0xB02,
    mhpmcounter3=0xB03,
    mhpmcounter4=0xB04,
    /*...*/
    mhpmcounter31=0xB1F,
    mcycleh=0xB80,
    minstreth=0xB82,
    mhpmcounter3h=0xB83,
    mhpmcounter4h=0xB84,
    /*...*/
    mhpmcounter31h=0xB9F,
    // Machine Counter Setup
    mhpmevent3=0x323,
    mhpmevent4=0x324,
    /*...*/
    mhpmevent31=0x33F,
    // Debug/Trace Registers (shared with Debug Mode)
    tselect=0x7A0,
    tdata1=0x7A1,
    tdata2=0x7A2,
    tdata3=0x7A3,
    // Debug Mode Registers
    dcsr=0x7B0,
    dpc=0x7B1,
    dscratch=0x7B2
};

char lvl[]={'U', 'S', 'H', 'M'};

const char* trap_str[] = {
        "Instruction address misaligned",
        "Instruction access fault",
        "Illegal instruction",
        "Breakpoint",
        "Load address misaligned",
        "Load access fault",
        "Store/AMO address misaligned",
        "Store/AMO access fault",
        "Environment call from U-mode",
        "Environment call from S-mode",
        "Reserved",
        "Environment call from M-mode",
        "Instruction page fault",
        "Load page fault",
        "Reserved",
        "Store/AMO page fault"
};
const char* irq_str[] = {
        "User software interrupt",
        "Supervisor software interrupt",
        "Reserved",
        "Machine software interrupt",
        "User timer interrupt",
        "Supervisor timer interrupt",
        "Reserved",
        "Machine timer interrupt",
        "User external interrupt",
        "Supervisor external interrupt",
        "Reserved",
        "Machine external interrupt"
};

namespace {
enum {
    PGSHIFT=12,
    PTE_PPN_SHIFT=10,
    // page table entry (PTE) fields
    PTE_V    = 0x001, // Valid
    PTE_R    = 0x002, // Read
    PTE_W    = 0x004, // Write
    PTE_X    = 0x008, // Execute
    PTE_U    = 0x010, // User
    PTE_G    = 0x020, // Global
    PTE_A    = 0x040, // Accessed
    PTE_D    = 0x080, // Dirty
    PTE_SOFT = 0x300 // Reserved for Software
};

template<typename T>
inline bool PTE_TABLE(T PTE){
    return (((PTE) & (PTE_V | PTE_R | PTE_W | PTE_X)) == PTE_V);
}


enum { PRIV_U=0, PRIV_S=1, PRIV_M=3};

enum {
    ISA_A=1,
    ISA_B=1<<1,
    ISA_C=1<<2,
    ISA_D=1<<3,
    ISA_E=1<<4,
    ISA_F=1<<5,
    ISA_G=1<<6,
    ISA_I=1<<8,
    ISA_M=1<<12,
    ISA_N=1<<13,
    ISA_Q=1<<16,
    ISA_S=1<<18,
    ISA_U=1<<20};

struct vm_info {
    int levels;
    int idxbits;
    int ptesize;
    uint64_t ptbase;
};


struct trap_load_access_fault: public trap_access {
    trap_load_access_fault(uint64_t badaddr) : trap_access(5<<16, badaddr) {}
};
struct illegal_instruction_fault: public trap_access {
    illegal_instruction_fault(uint64_t badaddr) : trap_access(2<<16, badaddr) {}
};
struct trap_instruction_page_fault: public trap_access {
    trap_instruction_page_fault(uint64_t badaddr) : trap_access(12<<16, badaddr) {}
};
struct trap_load_page_fault: public trap_access {
    trap_load_page_fault(uint64_t badaddr) : trap_access(13<<16, badaddr) {}
};
struct trap_store_page_fault: public trap_access {
    trap_store_page_fault(uint64_t badaddr) : trap_access(15<<16, badaddr) {}
};
}

typedef union {
    uint32_t val;
    struct /*mstatus*/ {
        uint32_t
        SD:1,     //SD bit is read-only and is set when either the FS or XS bits encode a Dirty state (i.e., SD=((FS==11) OR (XS==11)))
        _WPRI3:8, //unused
        TSR:1,    //Trap SRET
        TW:1,     //Timeout Wait
        TVM:1,    //Trap Virtual Memory
        MXR:1,    //Make eXecutable Readable
        SUM:1,    //permit Supervisor User Memory access
        MPRV:1,   //Modify PRiVilege
        XS:2,     //status of additional user-mode extensions and associated state, All off/None dirty or clean, some on/None dirty, some clean/Some dirty
        FS:2,     //floating-point unit status Off/Initial/Clean/Dirty
        MPP:2,    // machine previous privilege
        _WPRI2:2, // unused
        SPP:1,    // supervisor previous privilege
        MPIE:1,   //previous machine interrupt-enable
        _WPRI1:1, // unused
        SPIE:1,   //previous supervisor interrupt-enable
        UPIE:1,   //previous user interrupt-enable
        MIE:1,    //machine interrupt-enable
        _WPRI0:1, // unused
        SIE:1,    //supervisor interrupt-enable
        UIE:1;    //user interrupt-enable
    } m;
    struct /*sstatus*/ {
        uint32_t
        SD:1,
        _WPRI4:11,
        MXR:1,
        SUM:1,
        _WPRI3:1,
        XS:2,
        FS:2,
        _WPRI2:4,
        SPP:1,
        _WPRI1:2,
        SPIE:1,
        UPIE:1,
        _WPRI0:2,
        SIE:1,
        UIE:1;
    } s;
    struct /*ustatus*/ {
        uint32_t
        SD:1,
        _WPRI4:11,
        MXR:1,
        SUM:1,
        _WPRI3:1,
        XS:2,
        FS:2,
        _WPRI2:8,
        UPIE:1,
        _WPRI0:3,
        UIE:1;
    } u;
} mstatus32_t;

typedef union {
    uint64_t val;
    struct /*mstatus*/ {
        uint64_t
        SD:1,     // SD bit is read-only and is set when either the FS or XS bits encode a Dirty state (i.e., SD=((FS==11) OR (XS==11)))
        _WPRI4:27,// unused
        SXL:2,    // value of XLEN for S-mode
        UXL:2,    // value of XLEN for U-mode
        _WPRI3:9, // unused
        TSR:1,    // Trap SRET
        TW:1,     // Timeout Wait
        TVM:1,    // Trap Virtual Memory
        MXR:1,    // Make eXecutable Readable
        SUM:1,    // permit Supervisor User Memory access
        MPRV:1,   // Modify PRiVilege
        XS:2,     // status of additional user-mode extensions and associated state, All off/None dirty or clean, some on/None dirty, some clean/Some dirty
        FS:2,     // floating-point unit status Off/Initial/Clean/Dirty
        MPP:2,    // machine previous privilege
        _WPRI2:2, // unused
        SPP:1,    // supervisor previous privilege
        MPIE:1,   // previous machine interrupt-enable
        _WPRI1:1, // unused
        SPIE:1,   // previous supervisor interrupt-enable
        UPIE:1,   // previous user interrupt-enable
        MIE:1,    // machine interrupt-enable
        _WPRI0:1, // unused
        SIE:1,    // supervisor interrupt-enable
        UIE:1;    // ‚user interrupt-enable
    } m;
    struct /*sstatus*/ {
        uint64_t
        SD:1,
        _WPRI5:29,// unused
        UXL:2,    // value of XLEN for U-mode
        _WPRI4:12,
        MXR:1,
        SUM:1,
        _WPRI3:1,
        XS:2,
        FS:2,
        _WPRI2:4,
        SPP:1,
        _WPRI1:2,
        SPIE:1,
        UPIE:1,
        _WPRI0:2,
        SIE:1,
        UIE:1;
    } s;
    struct /*ustatus*/ {
        uint32_t
        SD:1,
        _WPRI4:29,// unused
        UXL:2,    // value of XLEN for U-mode
        _WPRI3:12,
        MXR:1,
        SUM:1,
        _WPRI2:1,
        XS:2,
        FS:2,
        _WPRI1:8,
        UPIE:1,
        _WPRI0:3,
        UIE:1;
    } u;
} mstatus64_t;

template<unsigned L>
inline vm_info decode_vm_info(uint32_t state, uint64_t sptbr);

template<>
inline vm_info decode_vm_info<32u>(uint32_t state, uint64_t sptbr){
    if (state == PRIV_M) {
        return {0, 0, 0, 0};
    } else if (state <= PRIV_S) {
        switch (bit_sub<31,1>(sptbr)) {
        case 0: // off
            return {0, 0, 0, 0};
        case 1: // SV32
            return {2, 10, 4, bit_sub<0, 22>(sptbr) << PGSHIFT};
        default: abort();
        }
    } else {
        abort();
    }
    return {0, 0, 0, 0}; // dummy
}

template<>
inline vm_info decode_vm_info<64u>(uint32_t state, uint64_t sptbr){
    if (state == PRIV_M) {
        return {0, 0, 0, 0};
    } else if (state <= PRIV_S) {
        switch (bit_sub<60, 4>(sptbr)) {
        case 0: // off
            return {0, 0, 0, 0};
        case 8: // SV39
            return {3, 9, 8, bit_sub<0, 44>(sptbr) << PGSHIFT};
        case 9: // SV48
            return {4, 9, 8, bit_sub<0, 44>(sptbr) << PGSHIFT};
        case 10: // SV57
            return {5, 9, 8, bit_sub<0, 44>(sptbr) << PGSHIFT};
        case 11: // SV64
            return {6, 9, 8, bit_sub<0, 44>(sptbr) << PGSHIFT};
        default: abort();
        }
    } else {
        abort();
    }
    return {0, 0, 0, 0}; // dummy
}


constexpr uint32_t get_mask(unsigned priv_lvl, uint32_t mask){
    switch(priv_lvl){
    case PRIV_U:
        return mask&0x80000011UL; //           0b1000 0000 0000 0000 0000 0000 0001 0001
    case PRIV_S:
        return mask&0x800de133UL; //           0b1000 0000 0000 1101 1110 0001 0011 0011
    default:
        return mask&0x807ff9ddUL; //           0b1000 0000 0111 1111 1111 1001 1011 1011
    }
}

constexpr uint64_t  get_mask(unsigned priv_lvl, uint64_t mask){
    switch(priv_lvl){
    case PRIV_U:
        return mask&0x8000000000000011ULL; //0b1...0 1111 0000 0000 0111 1111 1111 1001 1011 1011
    case PRIV_S:
        return mask&0x80000003000de133ULL; //0b1...0 0011 0000 0000 0000 1101 1110 0001 0011 0011
    default:
        return mask&0x8000000f007ff9ddULL; //0b1...0 1111 0000 0000 0111 1111 1111 1001 1011 1011
    }
}

constexpr uint32_t get_misa(uint32_t mask){
    return (1UL<<30)| ISA_I | ISA_M | ISA_A | ISA_U | ISA_S | ISA_M ;
}

constexpr uint64_t get_misa(uint64_t mask){
    return (2ULL<<62)| ISA_I | ISA_M | ISA_A | ISA_U | ISA_S | ISA_M ;
}

template<typename BASE>
struct riscv_core: public BASE {
    using super = BASE;
    using this_class = riscv_core<BASE>;
    using virt_addr_t= typename super::virt_addr_t;
    using phys_addr_t= typename super::phys_addr_t;
    using reg_t =  typename super::reg_t;
    using addr_t = typename super::addr_t;

    using rd_csr_f = iss::status (this_class::*)(unsigned addr, reg_t&);
    using wr_csr_f = iss::status (this_class::*)(unsigned addr, reg_t);

    const typename super::reg_t PGSIZE = 1 << PGSHIFT;
    const typename super::reg_t PGMASK = PGSIZE-1;

    constexpr reg_t get_irq_mask(size_t mode){
        const reg_t m[4] = {
                0b000100010001, //U mode
                0b001100110011, // S-mode
                0,
                0b101110111011 // M-mode
        };
        return m[mode];
    }

    riscv_core();
    virtual ~riscv_core();

    virtual void load_file(std::string name, int type=-1);

    virtual phys_addr_t v2p(const iss::addr_t& addr);

    virtual iss::status read(const iss::addr_t& addr, unsigned length, uint8_t* const data) override;
    virtual iss::status write(const iss::addr_t& addr, unsigned length, const uint8_t* const data) override;

    virtual uint64_t enter_trap(uint64_t flags) override {return riscv_core::enter_trap(flags, fault_data);}
    virtual uint64_t enter_trap(uint64_t flags, uint64_t addr) override;
    virtual uint64_t leave_trap(uint64_t flags) override;
    virtual void wait_until(uint64_t flags) override;

    virtual std::string get_additional_disass_info(){
        std::stringstream s;
        auto status = csr[mstatus];
        s<<"[p:"<<lvl[this->reg.machine_state]<<";s:0x"<<std::hex<<std::setfill('0')<<std::setw(sizeof(reg_t)*2)<<status<<std::dec<<";c:"<<this->reg.icount<<"]";
        return s.str();
    };

protected:
    virtual iss::status read_mem(phys_addr_t addr, unsigned length, uint8_t* const data);
    virtual iss::status write_mem(phys_addr_t addr, unsigned length, const uint8_t* const data);

    virtual iss::status read_csr(unsigned addr, reg_t& val);
    virtual iss::status write_csr(unsigned addr, reg_t  val);

    uint64_t tohost = tohost_dflt;
    uint64_t fromhost = fromhost_dflt;

    reg_t fault_data;
    using mem_type = util::sparse_array<uint8_t, 1ULL<<32>;
    using csr_type = util::sparse_array<typename  traits<BASE>::reg_t, 1ULL<<12, 12>;
    using csr_page_type = typename csr_type::page_type;
    mem_type mem;
    csr_type csr;
    unsigned to_host_wr_cnt=0;
    std::stringstream uart_buf;
    std::unordered_map<reg_t, uint64_t> ptw;
    std::unordered_map<uint64_t, uint8_t> atomic_reservation;
    std::unordered_map<unsigned, rd_csr_f> csr_rd_cb;
    std::unordered_map<unsigned, wr_csr_f> csr_wr_cb;

private:
    iss::status read_cycle(unsigned addr, reg_t& val);
    iss::status read_status(unsigned addr, reg_t& val);
    iss::status write_status(unsigned addr, reg_t val);
    iss::status read_ie(unsigned addr, reg_t& val);
    iss::status write_ie(unsigned addr, reg_t val);
    iss::status read_ip(unsigned addr, reg_t& val);
    iss::status write_ip(unsigned addr, reg_t val);
    iss::status read_satp(unsigned addr, reg_t& val);
    iss::status write_satp(unsigned addr, reg_t val);
    void check_interrupt();
};

template<typename BASE>
riscv_core<BASE>::riscv_core() {
    csr[misa]=traits<BASE>::XLEN==32?1ULL<<(traits<BASE>::XLEN-2):2ULL<<(traits<BASE>::XLEN-2);
    uart_buf.str("");
    // read-only registers
    csr_wr_cb[misa]=nullptr;
    for(unsigned addr=mcycle; addr<=hpmcounter31; ++addr)
        csr_wr_cb[addr]=nullptr;
    for(unsigned addr=mcycleh; addr<=hpmcounter31h; ++addr)
        csr_wr_cb[addr]=nullptr;
    // special handling
    csr_rd_cb[mcycle]=&riscv_core<BASE>::read_cycle;
    csr_rd_cb[mcycleh]=&riscv_core<BASE>::read_cycle;
    csr_rd_cb[minstret]=&riscv_core<BASE>::read_cycle;
    csr_rd_cb[minstreth]=&riscv_core<BASE>::read_cycle;
    csr_rd_cb[mstatus]=&riscv_core<BASE>::read_status;
    csr_wr_cb[mstatus]=&riscv_core<BASE>::write_status;
    csr_rd_cb[sstatus]=&riscv_core<BASE>::read_status;
    csr_wr_cb[sstatus]=&riscv_core<BASE>::write_status;
    csr_rd_cb[ustatus]=&riscv_core<BASE>::read_status;
    csr_wr_cb[ustatus]=&riscv_core<BASE>::write_status;
    csr_rd_cb[mip]=&riscv_core<BASE>::read_ip;
    csr_wr_cb[mip]=&riscv_core<BASE>::write_ip;
    csr_rd_cb[sip]=&riscv_core<BASE>::read_ip;
    csr_wr_cb[sip]=&riscv_core<BASE>::write_ip;
    csr_rd_cb[uip]=&riscv_core<BASE>::read_ip;
    csr_wr_cb[uip]=&riscv_core<BASE>::write_ip;
    csr_rd_cb[mie]=&riscv_core<BASE>::read_ie;
    csr_wr_cb[mie]=&riscv_core<BASE>::write_ie;
    csr_rd_cb[sie]=&riscv_core<BASE>::read_ie;
    csr_wr_cb[sie]=&riscv_core<BASE>::write_ie;
    csr_rd_cb[uie]=&riscv_core<BASE>::read_ie;
    csr_wr_cb[uie]=&riscv_core<BASE>::write_ie;
    csr_rd_cb[satp]=&riscv_core<BASE>::read_satp;
    csr_wr_cb[satp]=&riscv_core<BASE>::write_satp;
}

template<typename BASE>
riscv_core<BASE>::~riscv_core() {
}

template<typename BASE>
void riscv_core<BASE>::load_file(std::string name, int type) {
    FILE* fp = fopen(name.c_str(), "r");
    if(fp){
        char buf[5];
        auto n = fread(buf, 1,4,fp);
        if(n!=4) throw std::runtime_error("input file has insufficient size");
        buf[4]=0;
        if(strcmp(buf+1, "ELF")==0){
            fclose(fp);
            //Create elfio reader
            ELFIO::elfio reader;
            // Load ELF data
            if ( !reader.load( name ) ) throw std::runtime_error("could not process elf file");
            // check elf properties
            //TODO: fix ELFCLASS like:
            // if ( reader.get_class() != ELFCLASS32 ) throw std::runtime_error("wrong elf class in file");
            if ( reader.get_type() != ET_EXEC ) throw std::runtime_error("wrong elf type in file");
            //TODO: fix machine type like:
            // if ( reader.get_machine() != EM_RISCV ) throw std::runtime_error("wrong elf machine in file");
            for (const auto pseg :reader.segments ) {
                const auto fsize=pseg->get_file_size();         // 0x42c/0x0
                const auto seg_data=pseg->get_data();
                if(fsize>0){
                    this->write(typed_addr_t<PHYSICAL>(iss::DEBUG_WRITE, traits<minrv_ima>::MEM, pseg->get_virtual_address()), fsize, reinterpret_cast<const uint8_t* const>(seg_data));
                }
            }
            for (const auto sec :reader.sections ) {
                if(sec->get_name() == ".tohost"){
                    tohost=sec->get_address();
                    fromhost=tohost+0x40;
                }
            }
            return;
        }
    }
}

template<typename BASE>
iss::status riscv_core<BASE>::read(const iss::addr_t& addr, unsigned length, uint8_t* const data){
#ifndef NDEBUG
    if(addr.type& iss::DEBUG){
        LOG(DEBUG)<<"debug read of "<<length<<" bytes @addr "<<addr;
    } else {
        LOG(DEBUG)<<"read of "<<length<<" bytes  @addr "<<addr;
    }
#endif
    switch(addr.space){
    case traits<BASE>::MEM:{
        if((addr.type&(iss::ACCESS_TYPE-iss::DEBUG))==iss::FETCH &&  (addr.val&0x1) == 1){
            fault_data=addr.val;
            if((addr.type&iss::DEBUG))
                throw trap_access(0, addr.val);
            this->reg.trap_state=(1<<31); // issue trap 0
            return iss::Err;
        }
        try {
            if((addr.val&~PGMASK) != ((addr.val+length-1)&~PGMASK)){ // we may cross a page boundary
                vm_info vm = decode_vm_info<traits<BASE>::XLEN>(this->reg.machine_state, csr[satp]);
                if(vm.levels!=0){ // VM is active
                    auto split_addr = (addr.val+length)&~PGMASK;
                    auto len1=split_addr-addr.val;
                    auto res = read(addr, len1, data);
                    if(res==iss::Ok)
                        res = read(iss::addr_t{addr.type, addr.space, split_addr}, length-len1, data+len1);
                    return res;
                }
            }
            phys_addr_t paddr = (addr.type&iss::ADDRESS_TYPE)==iss::PHYSICAL?addr:v2p(addr);
            if((paddr.val +length)>mem.size()) return iss::Err;
            switch(paddr.val){
            case 0x0200BFF8:{ // CLINT base, mtime reg
                uint64_t mtime = this->reg.icount>>12/*12*/;
                std::copy((uint8_t*)&mtime, ((uint8_t*)&mtime)+length, data);
            }
            break;
            case 0x10008000:{
                const mem_type::page_type& p = mem(paddr.val/mem.page_size);
                uint64_t offs=paddr.val&mem.page_addr_mask;
                std::copy(
                        p.data() + offs,
                        p.data() + offs+length,
                        data);
                if(this->reg.icount>30000)
                    data[3]|=0x80;
            }
            break;
            default:{
                return read_mem(paddr,  length, data);
            }
            }
        } catch(trap_access& ta){
            this->reg.trap_state=(1<<31)|ta.id;
            return iss::Err;
        }
    }
    break;
    case traits<BASE>::CSR:{
        if(length!=sizeof(reg_t)) return iss::Err;
        return read_csr(addr.val, *reinterpret_cast<reg_t* const>(data));
    }
    break;
    case traits<BASE>::FENCE:{
        if((addr.val +length)>mem.size()) return iss::Err;
        switch(addr.val){
        case 2: // SFENCE:VMA lower
        case 3:{// SFENCE:VMA upper
            auto status = csr[mstatus];
            auto tvm = status&(1<<20);
            if(this->reg.machine_state==PRIV_S & tvm!=0){
                this->reg.trap_state=(1<<31)|(2<<16);
                this->fault_data=this->reg.PC;
                return iss::Err;
            }
            return iss::Ok;
        }
        }
    }
    break;
    case traits<BASE>::RES:{
        auto it = atomic_reservation.find(addr.val);
        if(it!= atomic_reservation.end() && (*it).second != 0){
            memset(data, 0xff, length);
            atomic_reservation.erase(addr.val);
        } else
            memset(data, 0, length);
    }
    break;
    default:
        return iss::Err; //assert("Not supported");
    }
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::write(const iss::addr_t& addr, unsigned length, const uint8_t* const data){
#ifndef NDEBUG
    const char* prefix = addr.type & iss::DEBUG?"debug ":"";
    switch(length){
    case 8:
        LOG(DEBUG)<<prefix<<"write of "<<length<<" bytes (0x"<<std::hex<<*(uint64_t*)&data[0]<<std::dec<<") @addr "<<addr;
        break;
    case 4:
        LOG(DEBUG)<<prefix<<"write of "<<length<<" bytes (0x"<<std::hex<<*(uint32_t*)&data[0]<<std::dec<<") @addr "<<addr;
        break;
    case 2:
        LOG(DEBUG)<<prefix<<"write of "<<length<<" bytes (0x"<<std::hex<<*(uint16_t*)&data[0]<<std::dec<<") @addr "<<addr;
        break;
    case 1:
        LOG(DEBUG)<<prefix<<"write of "<<length<<" bytes (0x"<<std::hex<<(uint16_t)data[0]<<std::dec<<") @addr "<<addr;
        break;
    default:
        LOG(DEBUG)<<prefix<<"write of "<<length<<" bytes @addr "<<addr;
    }
#endif
    try {
        switch(addr.space){
        case traits<BASE>::MEM:{
            phys_addr_t paddr = (addr.type&iss::ADDRESS_TYPE)==iss::PHYSICAL?addr:v2p(addr);
            if((paddr.val +length)>mem.size()) return iss::Err;
            switch(paddr.val){
            case 0x10013000: // UART0 base, TXFIFO reg
            case 0x10023000: // UART1 base, TXFIFO reg
                uart_buf<<(char)data[0];
                if(((char)data[0])=='\n' || data[0]==0){
                    // LOG(INFO)<<"UART"<<((paddr.val>>16)&0x3)<<" send '"<<uart_buf.str()<<"'";
                    std::cout<<uart_buf.str();
                    uart_buf.str("");
                }
                return iss::Ok;
            case 0x10008000:{ // HFROSC base, hfrosccfg reg
                mem_type::page_type& p = mem(paddr.val/mem.page_size);
                size_t offs=paddr.val&mem.page_addr_mask;
                std::copy(data, data+length, p.data()+offs);
                uint8_t& x = *(p.data()+offs+3);
                if(x&0x40) x|=0x80; // hfroscrdy = 1 if hfroscen==1
                return iss::Ok;
            }
            case 0x10008008:{ // HFROSC base, pllcfg reg
                mem_type::page_type& p = mem(paddr.val/mem.page_size);
                size_t offs=paddr.val&mem.page_addr_mask;
                std::copy(data, data+length, p.data()+offs);
                uint8_t& x = *(p.data()+offs+3);
                x|=0x80; // set pll lock upon writing
                return iss::Ok;
            }
            break;
            default:{
                return write_mem(paddr, length, data);
            }
            }
        }
        break;
        case traits<BASE>::CSR:{
            if(length!=sizeof(reg_t)) return iss::Err;
            return write_csr(addr.val, *reinterpret_cast<const reg_t*>(data));
        }
        break;
        case traits<BASE>::FENCE:{
            if((addr.val +length)>mem.size()) return iss::Err;
            switch(addr.val){
            case 2:
            case 3:{
                ptw.clear();
                auto status = csr[mstatus];
                auto tvm = status&(1<<20);
                if(this->reg.machine_state==PRIV_S & tvm!=0){
                    this->reg.trap_state=(1<<31)|(2<<16);
                    this->fault_data=this->reg.PC;
                    return iss::Err;
                }
                return iss::Ok;
            }
            }
        }
        break;
        case traits<BASE>::RES:{
            atomic_reservation[addr.val] = data[0];
        }
        break;
        default:
            return iss::Err;
        }
        return iss::Ok;
    } catch(trap_access& ta){
        this->reg.trap_state=(1<<31)|ta.id;
        return iss::Err;
    }
}

template<typename BASE>
iss::status riscv_core<BASE>::read_csr(unsigned addr, reg_t& val){
    if(addr >= csr.size()) return iss::Err;
    auto it = csr_rd_cb.find(addr);
    if(it == csr_rd_cb.end()){
        val=csr[addr&csr.page_addr_mask];
        return iss::Ok;
    }
    rd_csr_f f=it->second;
    if(f==nullptr)
        throw illegal_instruction_fault(this->fault_data);
    return (this->*f)(addr, val);
}

template<typename BASE>
iss::status riscv_core<BASE>::write_csr(unsigned addr, reg_t val){
    if(addr>=csr.size()) return iss::Err;
    auto it = csr_wr_cb.find(addr);
    if(it == csr_wr_cb.end()){
        csr[addr&csr.page_addr_mask] = val;
        return iss::Ok;
    }
    wr_csr_f f=it->second;
    if(f==nullptr)
        throw illegal_instruction_fault(this->fault_data);
    return (this->*f)(addr, val);

}

template<typename BASE>
iss::status riscv_core<BASE>::read_cycle(unsigned addr, reg_t& val) {
    if( addr== mcycle) {
        val = static_cast<reg_t>(this->reg.icount);
    }else if(addr==mcycleh) {
        if(sizeof(typename  traits<BASE>::reg_t)!=4) return iss::Err;
        val = static_cast<reg_t>((this->reg.icount)>>32);
    }
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::read_status(unsigned addr, reg_t& val) {
    auto req_priv_lvl=addr>>8;
    if(this->reg.machine_state<req_priv_lvl) throw illegal_instruction_fault(this->fault_data);
    auto mask = get_mask(req_priv_lvl, (reg_t) (std::numeric_limits<reg_t>::max()));
    val = csr[mstatus] & mask;
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::write_status(unsigned addr, reg_t val) {
    auto req_priv_lvl=addr>>8;
    if(this->reg.machine_state<req_priv_lvl) throw illegal_instruction_fault(this->fault_data);
    auto mask=get_mask(req_priv_lvl, (reg_t)std::numeric_limits<reg_t>::max());
    auto old_val=csr[mstatus];
    auto new_val = (old_val&~mask) |(val&mask);
    csr[mstatus] = new_val;
    check_interrupt();
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::read_ie(unsigned addr, reg_t& val) {
    auto req_priv_lvl=addr>>8;
    if(this->reg.machine_state<req_priv_lvl) throw illegal_instruction_fault(this->fault_data);
    val = csr[mie];
    if(addr<mie) val &= csr[mideleg];
    if(addr<sie) val &= csr[sideleg];
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::write_ie(unsigned addr, reg_t val) {
    auto req_priv_lvl=addr>>8;
    if(this->reg.machine_state<req_priv_lvl) throw illegal_instruction_fault(this->fault_data);
    auto mask=get_irq_mask(req_priv_lvl);
    csr[mie] = (csr[mie] & ~mask) | (val & mask);
    check_interrupt();
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::read_ip(unsigned addr, reg_t& val) {
    auto req_priv_lvl=addr>>8;
    if(this->reg.machine_state<req_priv_lvl) throw illegal_instruction_fault(this->fault_data);
    val = csr[mie];
    if(addr<mie) val &= csr[mideleg];
    if(addr<sie) val &= csr[sideleg];
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::write_ip(unsigned addr, reg_t val) {
    auto req_priv_lvl=addr>>8;
    if(this->reg.machine_state<req_priv_lvl) throw illegal_instruction_fault(this->fault_data);
    auto mask=get_irq_mask(req_priv_lvl);
    csr[mip] = (csr[mip] & ~mask) | (val & mask);
    check_interrupt();
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::read_satp(unsigned addr, reg_t& val){
    auto status = csr[mstatus];
    auto tvm = status&(1<<20);
    if(this->reg.machine_state==PRIV_S & tvm!=0){
        this->reg.trap_state=(1<<31)|(2<<16);
        this->fault_data=this->reg.PC;
        return iss::Err;
    }
    val = csr[satp];
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::write_satp(unsigned addr, reg_t val){
    auto status = csr[mstatus];
    auto tvm = status&(1<<20);
    if(this->reg.machine_state==PRIV_S & tvm!=0){
        this->reg.trap_state=(1<<31)|(2<<16);
        this->fault_data=this->reg.PC;
        return iss::Err;
    }
    csr[satp] = val;
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::read_mem(phys_addr_t addr, unsigned length, uint8_t* const data) {
    const auto& p = mem(addr.val/mem.page_size);
    auto offs=addr.val&mem.page_addr_mask;
    std::copy(p.data() + offs, p.data() + offs+length, data);
    return iss::Ok;
}

template<typename BASE>
iss::status riscv_core<BASE>::write_mem(phys_addr_t addr, unsigned length, const uint8_t* const data) {
    mem_type::page_type& p = mem(addr.val/mem.page_size);
    std::copy(data, data+length, p.data()+(addr.val&mem.page_addr_mask));
    // tohost handling in case of riscv-test
    if((addr.type & iss::DEBUG)==0){
        auto tohost_upper = (traits<BASE>::XLEN==32 && addr.val == (tohost+4)) || (traits<BASE>::XLEN==64 && addr.val == tohost);
        auto tohost_lower = (traits<BASE>::XLEN==32 && addr.val == tohost) || (traits<BASE>::XLEN==64 && addr.val == tohost);
        if(tohost_lower || tohost_upper){
            uint64_t hostvar = *reinterpret_cast<uint64_t*>(p.data()+(tohost&mem.page_addr_mask));
            if(tohost_upper || (tohost_lower && to_host_wr_cnt>0)){
                switch(hostvar>>48){
                case 0:
                    (hostvar!=0x1?LOG(FATAL):LOG(INFO))<<"tohost value is 0x"<<std::hex<<hostvar<<std::dec<<
                    " ("<<hostvar<<"), stopping simulation";
                    throw(iss::simulation_stopped(hostvar));
                case 0x0101:{
                    char c = static_cast<char>(hostvar & 0xff);
                    if(c=='\n' || c==0){
                        LOG(INFO)<<"tohost send '"<<uart_buf.str()<<"'";
                        uart_buf.str("");
                    } else
                        uart_buf<<c;
                    to_host_wr_cnt=0;
                }
                break;
                default:
                    break;
                }
            } else
                if(tohost_lower) to_host_wr_cnt++;
        } else if((traits<BASE>::XLEN==32 && addr.val == fromhost+4) || (traits<BASE>::XLEN==64 && addr.val == fromhost)){
            uint64_t fhostvar = *reinterpret_cast<uint64_t*>(p.data()+(fromhost&mem.page_addr_mask));
            *reinterpret_cast<uint64_t*>(p.data()+(tohost&mem.page_addr_mask)) = fhostvar;
        }
    }
    return iss::Ok;
}

template<typename BASE>
void riscv_core<BASE>::check_interrupt(){
    auto status = csr[mstatus];
    auto ip = csr[mip];
    auto ie = csr[mie];
    auto ideleg = csr[mideleg];
    // Multiple simultaneous interrupts and traps at the same privilege level are handled in the following decreasing priority order:
    // external interrupts, software interrupts, timer interrupts, then finally any synchronous traps.
    auto ena_irq=ip&ie;

    auto mie = (csr[mstatus]>>3)&1;
    auto m_enabled = this->reg.machine_state < PRIV_M || (this->reg.machine_state == PRIV_M && mie);
    auto enabled_interrupts = m_enabled?ena_irq & ~ideleg :0;

    if (enabled_interrupts == 0){
        auto sie = (csr[mstatus]>>1)&1;
        auto s_enabled = this->reg.machine_state < PRIV_S || (this->reg.machine_state == PRIV_S && sie);
          enabled_interrupts = s_enabled?ena_irq & ideleg : 0;
    }
    if (enabled_interrupts!=0){
        int res = 0;
        while ((enabled_interrupts & 1) == 0)
            enabled_interrupts >>= 1, res++;
        this->reg.pending_trap = res<<16 | 1;
    }

}

template<typename BASE>
typename riscv_core<BASE>::phys_addr_t riscv_core<BASE>::v2p(const iss::addr_t& addr){
    const uint64_t tmp = reg_t(1) << (traits<BASE>::XLEN-1);
    const uint64_t msk = tmp | (tmp-1);

    if(addr.space!=traits<BASE>::MEM){ //non-memory access
        phys_addr_t ret(addr);
        ret.val &= msk;
        return ret;
    }

    const reg_t mstatus_r = csr[mstatus];
    const access_type type = (access_type)(addr.getAccessType()&~iss::DEBUG);
    uint32_t mode =type != iss::FETCH && bit_sub<17,1>(mstatus_r)? // MPRV
        mode = bit_sub<11,2>(mstatus_r):// MPV
        this->reg.machine_state;

    const vm_info vm = decode_vm_info<traits<BASE>::XLEN>(mode, csr[satp]);

    if (vm.levels == 0){
        phys_addr_t ret(addr);
        ret.val &= msk;
        return ret;
    }

    const bool s_mode = mode == PRIV_S;
    const bool sum = bit_sub<18,1>(mstatus_r); // MSTATUS_SUM);
    const bool mxr = bit_sub<19,1>(mstatus_r);// MSTATUS_MXR);

    auto it = ptw.find(addr.val >> PGSHIFT);
    if(it!=ptw.end()){
        const reg_t pte=it->second;
        const reg_t ad = PTE_A | ((type == iss::WRITE) * PTE_D);
#ifdef RISCV_ENABLE_DIRTY
        // set accessed and possibly dirty bits.
        *(uint32_t*)ppte |= ad;
        return {addr.getAccessType(), addr.space, (pte&(~PGMASK)) | (addr.val & PGMASK)};
#else
        // take exception if access or possibly dirty bit is not set.
        if ((pte & ad) == ad)
            return {addr.getAccessType(), addr.space, (pte&(~PGMASK)) | (addr.val & PGMASK)};
        else
            ptw.erase(it);
#endif
    } else {
        // verify bits xlen-1:va_bits-1 are all equal
        const int va_bits = PGSHIFT + vm.levels * vm.idxbits;
        const reg_t mask = (reg_t(1) << (traits<BASE>::XLEN> - (va_bits-1))) - 1;
        const reg_t masked_msbs = (addr.val >> (va_bits-1)) & mask;
        const int levels = (masked_msbs != 0 && masked_msbs != mask)? 0: vm.levels;

        reg_t base = vm.ptbase;
        for (int i = levels - 1; i >= 0; i--) {
            const int ptshift = i * vm.idxbits;
            const reg_t idx = (addr.val >> (PGSHIFT + ptshift)) & ((1 << vm.idxbits) - 1);

            // check that physical address of PTE is legal
            reg_t pte = 0;
            const uint8_t res = this->read(phys_addr_t(addr.getAccessType(), traits<BASE>::MEM, base + idx * vm.ptesize), vm.ptesize, (uint8_t*)&pte);
            if (res!=0)
                throw trap_load_access_fault(addr.val);
            const reg_t ppn = pte >> PTE_PPN_SHIFT;

            if (PTE_TABLE(pte)) { // next level of page table
                base = ppn << PGSHIFT;
            } else if ((pte & PTE_U) ? s_mode && (type == iss::FETCH || !sum) : !s_mode) {
                break;
            } else if (!(pte & PTE_V) || (!(pte & PTE_R) && (pte & PTE_W))) {
                break;
            } else if (type == iss::FETCH ? !(pte & PTE_X) :
                    type == iss::READ ?  !(pte & PTE_R) && !(mxr && (pte & PTE_X)) :
                            !((pte & PTE_R) && (pte & PTE_W))) {
                break;
            } else if ((ppn & ((reg_t(1) << ptshift) - 1)) != 0) {
                break;
            } else {
                const reg_t ad = PTE_A | ((type == iss::WRITE) * PTE_D);
#ifdef RISCV_ENABLE_DIRTY
                // set accessed and possibly dirty bits.
                *(uint32_t*)ppte |= ad;
#else
                // take exception if access or possibly dirty bit is not set.
                if ((pte & ad) != ad)
                    break;
#endif
                // for superpage mappings, make a fake leaf PTE for the TLB's benefit.
                const reg_t vpn = addr.val >> PGSHIFT;
                const reg_t value = (ppn | (vpn & ((reg_t(1) << ptshift) - 1))) << PGSHIFT;
                const reg_t offset = addr.val & PGMASK;
                ptw[vpn]=value | (pte&0xff);
                return {addr.getAccessType(), addr.space, value | offset};
            }
        }
    }
    switch (type) {
    case FETCH:
        this->fault_data=addr.val;
        throw trap_instruction_page_fault(addr.val);
    case READ:
        this->fault_data=addr.val;
        throw trap_load_page_fault(addr.val);
    case WRITE:
        this->fault_data=addr.val;
        throw trap_store_page_fault(addr.val);
    default: abort();
    }
}

template<typename BASE>
uint64_t riscv_core<BASE>::enter_trap(uint64_t flags, uint64_t addr) {
    auto cur_priv=this->reg.machine_state;
    // calculate and write mcause val
    auto trap_id=flags&0xffff;
    auto cause = (flags>>16)&0x7fff;
    if(trap_id==0 && cause==11) cause = 0x8+cur_priv; // adjust environment call cause
    // calculate effective privilege level
    auto new_priv=PRIV_M;
    if(trap_id==0){ // exception
        if(cur_priv!=PRIV_M && ((csr[medeleg]>>cause)&0x1)!=0)
            new_priv=(csr[sedeleg]>>cause)&0x1?PRIV_U:PRIV_S;
        // store ret addr in xepc register
        csr[uepc|(new_priv<<8)]=static_cast<reg_t>(addr); // store actual address instruction of exception
        /*
         * write mtval if new_priv=M_MODE, spec says:
         * When a hardware breakpoint is triggered, or an instruction-fetch, load, or store address-misaligned,
         * access, or page-fault exception occurs, mtval is written with the faulting effective address.
         */
        csr[utval|(new_priv<<8)]=fault_data;
        fault_data=0;
    }else{
        if(cur_priv!=PRIV_M && ((csr[mideleg]>>cause)&0x1)!=0)
            new_priv=(csr[sideleg]>>cause)&0x1?PRIV_U:PRIV_S;
        csr[uepc|(new_priv<<8)]=this->reg.NEXT_PC; // store next address if interrupt
        this->reg.pending_trap=0;
    }
    csr[ucause|(new_priv<<8)]=cause;
    // update mstatus
    // xPP field of mstatus is written with the active privilege mode at the time of the trap; the x PIE field of mstatus
    // is written with the value of the active interrupt-enable bit at the time of the trap; and the x IE field of mstatus
    // is cleared
    auto status=csr[mstatus];
    auto xie = (status>>cur_priv) & 1;
    // store the actual privilege level in yPP
    switch(new_priv){
    case PRIV_M:
        status&=~(3<<11);
        status|=(cur_priv&0x3)<<11;
        break;
    case PRIV_S:
        status&=~(1<<8);
        status|=(cur_priv&0x1)<<8;
        break;
    default:
        break;
    }
    // store interrupt enable flags
    status&=~(1<<(new_priv+4) | 1<<cur_priv); // clear respective xPIE and yIE
    status|= (xie<<(new_priv+4)); // store yIE

    csr[mstatus] = status;
    // get trap vector
    auto ivec = csr[utvec|(new_priv<<8)];
    // calculate addr// set NEXT_PC to trap addressess to jump to based on MODE bits in mtvec
    this->reg.NEXT_PC=ivec & ~0x1UL;
    if((ivec&0x1)==1 && trap_id!=0)
        this->reg.NEXT_PC+=4*cause;
    // reset trap state
    this->reg.machine_state=new_priv;
    this->reg.trap_state=0;
    char buffer[32];
    sprintf(buffer, "0x%016lx", addr);
    if(trap_id)
        el::Loggers::getLogger("disass", true)->info("Interrupt %v with cause '%v' at address %v occurred, changing privilege level from %v to %v",
            trap_id, irq_str[cause], buffer , lvl[cur_priv], lvl[new_priv]);
    else
        el::Loggers::getLogger("disass", true)->info("Trap %v with cause '%v' at address %v occurred, changing privilege level from %v to %v",
            trap_id, trap_str[cause], buffer , lvl[cur_priv], lvl[new_priv]);
    return this->reg.NEXT_PC;
}

template<typename BASE>
uint64_t riscv_core<BASE>::leave_trap(uint64_t flags) {
    auto cur_priv=this->reg.machine_state;
    auto inst_priv=flags&0x3;
    auto status=csr[mstatus];
    auto ppl = inst_priv; //previous privilege level

    auto tsr = status&(1<<22);
    if(cur_priv==PRIV_S && inst_priv==PRIV_S && tsr!=0){
        this->reg.trap_state=(1<<31)|(2<<16);
        this->fault_data=this->reg.PC;
        return this->reg.PC;
    }

    // pop the relevant lower-privilege interrupt enable and privilege mode stack
    switch(inst_priv){
    case PRIV_M:
        ppl=(status>>11)&0x3;
        status&=~(0x3<<11); // clear mpp to U mode
        break;
    case PRIV_S:
        ppl=(status>>8)&1;
        status&=~(1<<8); // clear spp to U mode
        break;
    case PRIV_U:
        ppl=0;
        break;
    }
    // sets the pc to the value stored in the x epc register.
    this->reg.NEXT_PC=csr[uepc|inst_priv<<8];
    status&=~(1<<ppl); // clear respective yIE
    auto pie=(status>>(inst_priv+4))&0x1; //previous interrupt enable
    status|= pie<<inst_priv; // and set the pie
    csr[mstatus]=status;
    this->reg.machine_state=ppl;
    el::Loggers::getLogger("disass", true)->info("Executing xRET , changing privilege level from %v to %v",
        lvl[cur_priv], lvl[ppl]);
    return this->reg.NEXT_PC;
}

template<typename BASE>
void riscv_core<BASE>::wait_until(uint64_t flags) {
    auto status=csr[mstatus];
    auto tw = status & (1<<21);
    if(this->reg.machine_state==PRIV_S && tw!=0){
        this->reg.trap_state=(1<<31)|(2<<16);
        this->fault_data=this->reg.PC;
    }
}
}
}

#endif /* _RISCV_CORE_H_ */