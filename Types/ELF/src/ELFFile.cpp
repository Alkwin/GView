#include "elf.hpp"

using namespace GView::Type::ELF;

ELFFile::ELFFile()
{
}

bool ELFFile::Update()
{
    panelsMask |= (1ULL << (uint8) Panels::IDs::Information);
    panelsMask |= (1ULL << (uint8) Panels::IDs::Segments);
    panelsMask |= (1ULL << (uint8) Panels::IDs::Sections);

    switch (is64 ? header64.e_machine : header32.e_machine)
    {
    case EM_386:
    case EM_486:
    case EM_860:
    case EM_960:
    case EM_8051:
    case EM_X86_64:
        panelsMask |= (1ULL << (uint8) Panels::IDs::OpCodes);
    default:
        break;
    }

    uint64 offset = 0;
    CHECK(obj->GetData().Copy<Elf32_Ehdr>(offset, header32), false, "");
    if (header32.e_ident[EI_CLASS] != ELFCLASS32)
    {
        CHECK(header32.e_ident[EI_CLASS] == ELFCLASS64, false, "");
        CHECK(obj->GetData().Copy<Elf64_Ehdr>(offset, header64), false, "");
        is64 = true;
        offset += sizeof(Elf64_Ehdr);
    }
    else
    {
        offset += sizeof(Elf32_Ehdr);
    }

    isLittleEndian = (header32.e_ident[EI_DATA] == ELFDATA2LSB);

    if (is64)
    {
        offset = header64.e_phoff;
        for (auto i = 0; i < header64.e_phnum; i++)
        {
            Elf64_Phdr entry{};
            CHECK(obj->GetData().Copy<Elf64_Phdr>(offset, entry), false, "");
            if ((entry.p_flags & PF_X) == PF_X)
            {
                executableZonesFAs.emplace_back(std::pair<uint64, uint64>{ entry.p_offset, entry.p_offset + entry.p_filesz });
            }
            segments64.emplace_back(entry);
            offset += sizeof(entry);
        }
    }
    else
    {
        offset = header32.e_phoff;
        for (auto i = 0; i < header32.e_phnum; i++)
        {
            Elf32_Phdr entry{};
            CHECK(obj->GetData().Copy<Elf32_Phdr>(offset, entry), false, "");
            if ((entry.p_flags & PF_X) == PF_X)
            {
                executableZonesFAs.emplace_back(std::pair<uint64, uint64>{ entry.p_offset, entry.p_offset + entry.p_filesz });
            }
            segments32.emplace_back(entry);
            offset += sizeof(entry);
        }
    }

    if (is64)
    {
        offset = header64.e_shoff;
        for (auto i = 0; i < header64.e_shnum; i++)
        {
            auto& entry = sections64.emplace_back(Elf64_Shdr{});
            CHECK(obj->GetData().Copy<Elf64_Shdr>(offset, entry), false, "");
            offset += sizeof(entry);

            auto& segmentIdx = sectionsToSegments.emplace_back(-1);
            for (auto i = 0; i < segments64.size(); i++)
            {
                const auto& segment = segments64.at(i);
                if (segment.p_vaddr != 0 && entry.sh_addr >= segment.p_vaddr &&
                    entry.sh_addr + entry.sh_size <= segment.p_vaddr + segment.p_filesz)
                {
                    segmentIdx = i;
                    break;
                }
            }
        }

        sectionNames.reserve(header64.e_shnum);

        if (header64.e_shstrndx != SHN_UNDEF && header64.e_shstrndx < SHN_LORESERVE)
        {
            const auto& shstrtab = sections64.at(header64.e_shstrndx);
            auto buffer          = obj->GetData().CopyToBuffer(shstrtab.sh_offset, (uint32) shstrtab.sh_size);
            if (buffer.IsValid())
            {
                for (const auto& section : sections64)
                {
                    const auto name = (char*) (buffer.GetData() + section.sh_name);
                    sectionNames.emplace_back(name);
                }
            }
        }
        else if (header64.e_shstrndx >= SHN_LORESERVE)
        {
            // TODO:
        }
    }
    else
    {
        offset = header32.e_shoff;
        for (auto i = 0; i < header32.e_shnum; i++)
        {
            auto& entry = sections32.emplace_back(Elf32_Shdr{});
            CHECK(obj->GetData().Copy<Elf32_Shdr>(offset, entry), false, "");
            offset += sizeof(entry);

            auto& segmentIdx = sectionsToSegments.emplace_back(-1);
            for (auto i = 0; i < segments32.size(); i++)
            {
                const auto& segment = segments32.at(i);
                if (segment.p_vaddr != 0 && entry.sh_addr >= segment.p_vaddr &&
                    entry.sh_addr + entry.sh_size <= segment.p_vaddr + segment.p_filesz)
                {
                    segmentIdx = i;
                    break;
                }
            }
        }

        sectionNames.reserve(header32.e_shnum);

        if (header32.e_shstrndx != SHN_UNDEF && header32.e_shstrndx < SHN_LORESERVE)
        {
            const auto& shstrtab = sections32.at(header32.e_shstrndx);
            auto buffer          = obj->GetData().CopyToBuffer(shstrtab.sh_offset, shstrtab.sh_size);
            if (buffer.IsValid())
            {
                for (const auto& section : sections32)
                {
                    const auto name = (char*) (buffer.GetData() + section.sh_name);
                    sectionNames.emplace_back(name);
                }
            }
        }
        else if (header32.e_shstrndx >= SHN_LORESERVE)
        {
            // TODO:
        }
    }

    CHECK(ParseGoData(), false, "");
    CHECK(ParseSymbols(), false, "");

    return true;
}

bool ELFFile::HasPanel(Panels::IDs id)
{
    return (panelsMask & (1ULL << ((uint8) id))) != 0;
}

bool ELFFile::ParseGoData()
{
    Buffer noteBuffer;
    if (is64)
    {
        for (const auto& segment : segments64)
        {
            if (segment.p_type == PT_NOTE)
            {
                noteBuffer = obj->GetData().CopyToBuffer(segment.p_offset, (uint32) segment.p_filesz);
            }
        }
    }
    else
    {
        for (const auto& segment : segments32)
        {
            if (segment.p_type == PT_NOTE)
            {
                noteBuffer = obj->GetData().CopyToBuffer(segment.p_offset, segment.p_filesz);
            }
        }
    }

    if (noteBuffer.IsValid() && noteBuffer.GetLength() >= 16)
    {
        nameSize = *(uint32*) noteBuffer.GetData();
        valSize  = *(uint32*) (noteBuffer.GetData() + 4);
        tag      = *(uint32*) (noteBuffer.GetData() + 8);
        noteName = std::string((char*) noteBuffer.GetData() + 12, 4);

        std::string_view noteNameView{ (char*) noteBuffer.GetData() + 12, 4 };
        if (nameSize == 4 && 16ULL + valSize <= noteBuffer.GetLength() && tag == Golang::ELF_GO_BUILD_ID_TAG &&
            noteNameView == Golang::ELF_GO_NOTE)
        {
            pcLnTab.SetBuildId({ (char*) noteBuffer.GetData() + 16, valSize });
        }

        if (nameSize == 4 && 16ULL + valSize <= noteBuffer.GetLength() && tag == Golang::GNU_BUILD_ID_TAG &&
            noteNameView == Golang::ELF_GNU_NOTE)
        {
            gnuString = std::string((char*) noteBuffer.GetData() + 16, valSize);
        }
    }

    // go symbols
    for (auto i = 0U; i < sectionNames.size(); i++)
    {
        auto& name = sectionNames.at(i);
        if (name == ".gopclntab")
        {
            panelsMask |= (1ULL << (uint8) Panels::IDs::GoInformation);

            uint64 bufferOffset       = 0;
            uint64 bufferSize         = 0;
            Golang::Architecture arch = is64 ? Golang::Architecture::x64 : Golang::Architecture::x86;
            if (is64)
            {
                const auto& section = sections64.at(i);
                bufferOffset        = section.sh_offset;
                bufferSize          = section.sh_size;
            }
            else
            {
                const auto& section = sections32.at(i);
                bufferOffset        = section.sh_offset;
                bufferSize          = section.sh_size;
            }

            CHECK(pcLnTab.Process(obj->GetData().CopyToBuffer(bufferOffset, (uint32) bufferSize), arch), false, "");
            break;
        }
    }

    return true;
}

bool ELFFile::ParseSymbols()
{
    if (is64)
    {
        for (auto i = 0; i < sections64.size(); i++)
        {
            const auto& section = sections64.at(i);
            if (section.sh_type == SHT_SYMTAB) /* Static symbol table */
            {
                panelsMask |= (1ULL << (uint8) Panels::IDs::StaticSymbols);

                const auto staticSymbolsBuffer = obj->GetData().CopyToBuffer(section.sh_offset, (uint32) section.sh_size);

                const auto& strtabSection = sections64.at(section.sh_link);
                const auto strtabBuffer   = obj->GetData().CopyToBuffer(strtabSection.sh_offset, (uint32) strtabSection.sh_size);

                auto offset = 0U;
                while (offset < section.sh_size)
                {
                    const auto& sym = staticSymbols64.emplace_back(*(Elf64_Sym*) (staticSymbolsBuffer.GetData() + offset));
                    offset += sizeof(Elf64_Sym);

                    String demangled;
                    const auto str = reinterpret_cast<char*>((char*) (strtabBuffer.GetData() + sym.st_name));
                    if (GView::Utils::Demangle(str, demangled) == false)
                    {
                        demangled = str;
                    }

                    staticSymbolsNames.emplace_back(demangled.GetText());
                }
            }
            else if (section.sh_type == SHT_DYNSYM) /* Dynamic symbol table */
            {
                panelsMask |= (1ULL << (uint8) Panels::IDs::DynamicSymbols);

                const auto dynamicSymbolsBuffer = obj->GetData().CopyToBuffer(section.sh_offset, (uint32) section.sh_size);

                const auto& dstrtabSection = sections64.at(section.sh_link);
                const auto dstrtabBuffer   = obj->GetData().CopyToBuffer(dstrtabSection.sh_offset, (uint32) dstrtabSection.sh_size);

                auto offset = 0U;
                while (offset < section.sh_size)
                {
                    const auto& sym = dynamicSymbols64.emplace_back(*(Elf64_Sym*) (dynamicSymbolsBuffer.GetData() + offset));
                    offset += sizeof(Elf64_Sym);

                    String demangled;
                    const auto str = reinterpret_cast<char*>((char*) (dstrtabBuffer.GetData() + sym.st_name));
                    if (GView::Utils::Demangle(str, demangled) == false)
                    {
                        demangled = str;
                    }

                    dynamicSymbolsNames.emplace_back(demangled.GetText());
                }
            }
        }
    }
    else
    {
        for (auto i = 0; i < sections32.size(); i++)
        {
            const auto& section = sections32.at(i);
            if (section.sh_type == SHT_SYMTAB) /* Static symbol table */
            {
                panelsMask |= (1ULL << (uint8) Panels::IDs::StaticSymbols);

                const auto staticSymbolsBuffer = obj->GetData().CopyToBuffer(section.sh_offset, section.sh_size);

                const auto& strtabSection = sections32.at(section.sh_link);
                const auto strtabBuffer   = obj->GetData().CopyToBuffer(strtabSection.sh_offset, strtabSection.sh_size);

                auto offset = 0U;
                while (offset < section.sh_size)
                {
                    const auto& sym = staticSymbols32.emplace_back(*(Elf32_Sym*) (staticSymbolsBuffer.GetData() + offset));
                    offset += sizeof(Elf32_Sym);

                    String demangled;
                    const auto str = reinterpret_cast<char*>((char*) (strtabBuffer.GetData() + sym.st_name));
                    if (GView::Utils::Demangle(str, demangled) == false)
                    {
                        demangled = str;
                    }

                    staticSymbolsNames.emplace_back(demangled.GetText());
                }
            }
            else if (section.sh_type == SHT_DYNSYM) /* Dynamic symbol table */
            {
                panelsMask |= (1ULL << (uint8) Panels::IDs::DynamicSymbols);

                const auto dynamicSymbolsBuffer = obj->GetData().CopyToBuffer(section.sh_offset, section.sh_size);

                const auto& dstrtabSection = sections32.at(section.sh_link);
                const auto dstrtabBuffer   = obj->GetData().CopyToBuffer(dstrtabSection.sh_offset, dstrtabSection.sh_size);

                auto offset = 0U;
                while (offset < section.sh_size)
                {
                    const auto& sym = dynamicSymbols32.emplace_back(*(Elf32_Sym*) (dynamicSymbolsBuffer.GetData() + offset));
                    offset += sizeof(Elf32_Sym);

                    String demangled;
                    const auto str = reinterpret_cast<char*>((char*) (dstrtabBuffer.GetData() + sym.st_name));
                    if (GView::Utils::Demangle(str, demangled) == false)
                    {
                        demangled = str;
                    }

                    dynamicSymbolsNames.emplace_back(demangled.GetText());
                }
            }
        }
    }

    return true;
}

uint64 ELFFile::TranslateToFileOffset(uint64 value, uint32 fromTranslationIndex)
{
    return ConvertAddress(value, static_cast<AddressType>(fromTranslationIndex), AddressType::FileOffset);
}

uint64 ELFFile::TranslateFromFileOffset(uint64 value, uint32 toTranslationIndex)
{
    return ConvertAddress(value, AddressType::FileOffset, static_cast<AddressType>(toTranslationIndex));
}

uint64 ELFFile::ConvertAddress(uint64 address, AddressType fromAddressType, AddressType toAddressType)
{
    switch (fromAddressType)
    {
    case AddressType::FileOffset:
        switch (toAddressType)
        {
        case AddressType::FileOffset:
            return address;
        case AddressType::VA:
            return FileOffsetToVA(address);
        };
        break;
    case AddressType::VA:
        switch (toAddressType)
        {
        case AddressType::FileOffset:
            return VAToFileOffset(address);
        case AddressType::VA:
            return address;
        };
        break;
    }

    return ELF_INVALID_ADDRESS;
}

uint64 ELFFile::FileOffsetToVA(uint64 fileOffset)
{
    if (is64)
    {
        for (const auto& section : sections64)
        {
            if (section.sh_offset <= fileOffset && fileOffset <= section.sh_offset + section.sh_size)
            {
                auto diff     = fileOffset - section.sh_offset;
                const auto fa = section.sh_addr + diff;
                return fa;
            }
        }
    }
    else
    {
        for (const auto& section : sections32)
        {
            if (section.sh_offset <= fileOffset && fileOffset <= section.sh_offset + section.sh_size)
            {
                auto diff     = fileOffset - section.sh_offset;
                const auto fa = section.sh_addr + diff;
                return fa;
            }
        }
    }

    return ELF_INVALID_ADDRESS;
}

uint64 ELFFile::VAToFileOffset(uint64 virtualAddress)
{
    if (is64)
    {
        for (const auto& section : sections64)
        {
            if (section.sh_addr != 0 && section.sh_addr <= virtualAddress && virtualAddress <= section.sh_addr + section.sh_size)
            {
                auto diff     = virtualAddress - section.sh_addr;
                const auto fa = section.sh_offset + diff;
                return fa;
            }
        }
    }
    else
    {
        for (const auto& section : sections32)
        {
            if (section.sh_addr != 0 && section.sh_addr <= virtualAddress && virtualAddress <= (uint64) section.sh_addr + section.sh_size)
            {
                auto diff     = virtualAddress - section.sh_addr;
                const auto fa = section.sh_offset + diff;
                return fa;
            }
        }
    }

    return ELF_INVALID_ADDRESS;
}

uint64 ELFFile::GetImageBase() const
{
    if (is64)
    {
        for (const auto& segment : segments64)
        {
            if (segment.p_type == PT_LOAD)
            {
                return segment.p_vaddr;
            }
        }
    }
    else
    {
        for (const auto& segment : segments32)
        {
            if (segment.p_type == PT_LOAD)
            {
                return segment.p_vaddr;
            }
        }
    }
    return -1;
}

uint64 ELFFile::GetVirtualSize() const
{
    uint64 vSize = 0;
    if (is64)
    {
        for (const auto& segment : segments64)
        {
            if (segment.p_type == PT_LOAD)
            {
                vSize += segment.p_memsz;
            }
        }
    }
    else
    {
        for (const auto& segment : segments32)
        {
            if (segment.p_type == PT_LOAD)
            {
                vSize += segment.p_memsz;
            }
        }
    }
    return -1;
}

bool ELFFile::GetColorForBufferIntel(uint64 offset, BufferView buf, GView::View::BufferViewer::BufferColor& result)
{
    CHECK(dissasembler.Init(is64, isLittleEndian), false, "");

    static auto previousOpcodeMask = showOpcodesMask;
    static std::map<uint64, GView::View::BufferViewer::BufferColor> cacheBuffer{};
    static std::map<uint64, bool> cacheDiscard{};

    if (previousOpcodeMask != showOpcodesMask)
    {
        cacheBuffer.clear();
        cacheDiscard.clear();
        previousOpcodeMask = showOpcodesMask;
    }

    if (cacheBuffer.count(offset) > 0)
    {
        result = cacheBuffer.at(offset);
        return true;
    }

    if (cacheDiscard.count(offset) > 0)
    {
        return false;
    }

    GView::Dissasembly::Instruction ins{ 0 };
    CHECK(dissasembler.DissasembleInstruction(buf, offset, ins), false, "");

    if (((showOpcodesMask & (uint32) GView::Dissasembly::Opcodes::Call) == (uint32) GView::Dissasembly::Opcodes::Call))
    {
        if (dissasembler.IsCallInstruction(ins))
        {
            result.start        = offset;
            result.end          = offset + ins.size;
            result.color        = INS_CALL_COLOR;
            cacheBuffer[offset] = result;
            return true;
        }
    }

    if (((showOpcodesMask & (uint32) GView::Dissasembly::Opcodes::LCall) == (uint32) GView::Dissasembly::Opcodes::LCall))
    {
        if (dissasembler.IsLCallInstruction(ins))
        {
            result.start        = offset;
            result.end          = offset + ins.size;
            result.color        = INS_LCALL_COLOR;
            cacheBuffer[offset] = result;
            return true;
        }
    }

    if (((showOpcodesMask & (uint32) GView::Dissasembly::Opcodes::Jmp) == (uint32) GView::Dissasembly::Opcodes::Jmp))
    {
        if (dissasembler.IsJmpInstruction(ins))
        {
            result.start        = offset;
            result.end          = offset + ins.size;
            result.color        = INS_JUMP_COLOR;
            cacheBuffer[offset] = result;
            return true;
        }
    }

    if (((showOpcodesMask & (uint32) GView::Dissasembly::Opcodes::LJmp) == (uint32) GView::Dissasembly::Opcodes::LJmp))
    {
        if (dissasembler.IsLJmpInstruction(ins))
        {
            result.start        = offset;
            result.end          = offset + ins.size;
            result.color        = INS_LJUMP_COLOR;
            cacheBuffer[offset] = result;
            return true;
        }
    }

    if (((showOpcodesMask & (uint32) GView::Dissasembly::Opcodes::Breakpoint) == (uint32) GView::Dissasembly::Opcodes::Breakpoint))
    {
        if (dissasembler.IsBreakpointInstruction(ins))
        {
            result.start        = offset;
            result.end          = offset + ins.size;
            result.color        = INS_BREAKPOINT_COLOR;
            cacheBuffer[offset] = result;
            return true;
        }
    }

    if (((showOpcodesMask & (uint32) GView::Dissasembly::Opcodes::FunctionStart) == (uint32) GView::Dissasembly::Opcodes::FunctionStart))
    {
        GView::Dissasembly::Instruction ins2{ 0 };
        const auto offset2 = offset + ins.size;
        CHECK(dissasembler.DissasembleInstruction({ buf.GetData() + ins.size, buf.GetLength() - ins.size }, offset2, ins2), false, "");

        if (dissasembler.AreFunctionStartInstructions(ins, ins2))
        {
            result.start        = offset;
            result.end          = offset + ins.size + ins2.size;
            result.color        = START_FUNCTION_COLOR;
            cacheBuffer[offset] = result;
            return true;
        }
    }

    if (((showOpcodesMask & (uint32) GView::Dissasembly::Opcodes::FunctionEnd) == (uint32) GView::Dissasembly::Opcodes::FunctionEnd))
    {
        if (dissasembler.IsFunctionEndInstruction(ins))
        {
            result.start        = offset;
            result.end          = offset + ins.size;
            result.color        = END_FUNCTION_COLOR;
            cacheBuffer[offset] = result;
            return true;
        }
    }

    cacheDiscard[offset] = false;

    return false;
}

bool ELFFile::GetColorForBuffer(uint64 offset, BufferView buf, GView::View::BufferViewer::BufferColor& result)
{
    CHECK(buf.IsValid(), false, "");
    result.color = ColorPair{ Color::Transparent, Color::Transparent };
    CHECK(showOpcodesMask != 0, false, "");

    const auto machine = is64 ? header64.e_machine : header32.e_machine;
    auto* p            = buf.begin();
    switch (*p)
    {
    case 0x7F:
        if (((showOpcodesMask & (uint32) GView::Dissasembly::Opcodes::Header) == (uint32) GView::Dissasembly::Opcodes::Header))
        {
            if (buf.GetLength() >= 4)
            {
                if (*(uint32*) p == 0x464C457F)
                {
                    result.start = offset;
                    result.end   = offset + 3;
                    result.color = EXE_MARKER_COLOR;
                    return true;
                } // do not break
            }
        }
    default:
        switch (machine)
        {
        case EM_386:
        case EM_486:
        case EM_860:
        case EM_960:
        case EM_8051:
        case EM_X86_64:
            for (const auto& [start, end] : executableZonesFAs)
            {
                if (offset >= start && offset < end)
                {
                    return GetColorForBufferIntel(offset, buf, result);
                }
            }
            break;
        default:
            break;
        }
    }

    return false;
}
