#include "code-injection-handler.h"
#include "process-memory-editor.h"
#include "aob-signature-ptr.h"
#include <cstring>
#include <exception>
#include <iostream>

CodeInjectionHandler::CodeInjectionHandler(
    const ProcessMemoryEditor& editor,
    const AOBSignaturePtr& ptr,
    size_t inj_size,
    const std::vector<std::byte>& instructions
)
{
    this->enabled = false;
    this->editor = &editor;

    this->ptr_size = this->editor->getPointerSize();
    this->inj_size = inj_size;
    // minimal number of bytes needed to replace for injection
    this->replace_size = 1 + this->ptr_size;

    if (this->inj_size < this->replace_size)
        throw std::exception("invalid injection length specified");

    // pointer to injection entry point
    this->regular_pointer = this->editor->getRegularPointer(ptr);

    // size of instructions to inject
    this->inj_instructions_size = instructions.size();

    // memory size to allocate for injected instructions
    // instructions size + JMP back size
    this->alloc_size = this->inj_instructions_size + 1 + this->ptr_size;
    this->inj_instructions = new std::byte[this->inj_instructions_size];

    for (size_t i = 0; i < this->inj_instructions_size; ++i)
    {
        this->inj_instructions[i] = instructions.at(i);
    }

    // absolute JMP 0xFF
    this->inj_instructions[this->inj_instructions_size] = (std::byte)0xFF;
    // absolute jump to next instruction after injection point
    std::memcpy(
        this->inj_instructions + this->inj_instructions_size + 1,
        (void*)(this->regular_pointer + this->inj_size),
        this->ptr_size
    );

    // injection point code
    this->new_jmp_instruction = new std::byte[this->inj_size];
    // NOP all extra bytes in case there are
    std::memset(this->new_jmp_instruction, 0x90, this->inj_size);
    this->jmp_addr = this->editor->allocate(this->alloc_size);
    std::memcpy(
        this->new_jmp_instruction + 1,
        (void*)this->jmp_addr,
        this->ptr_size
    );

    // original code
    this->saved_value = new std::byte[this->replace_size];
    this->editor->read_p(this->regular_pointer, this->saved_value, this->inj_size);

    this->new_jmp_instruction[0] = (std::byte)0xFF;

    this->editor->write_p(
        this->jmp_addr,
        this->inj_instructions,
        this->alloc_size
    );
}

void CodeInjectionHandler::enable()
{
    if (!this->enabled)
    {
        this->editor->write_p(this->regular_pointer, this->new_jmp_instruction, this->inj_size);
        this->enabled = true;
    }
}

void CodeInjectionHandler::disable()
{
    if (this->enabled)
    {
        this->editor->write_p(this->regular_pointer, this->saved_value, this->inj_size);
        this->enabled = false;
    }
}

CodeInjectionHandler::~CodeInjectionHandler()
{
    if (this->enabled)
    {
        this->disable();
    }

    this->editor->free(this->jmp_addr, this->inj_instructions_size);

    delete[] this->inj_instructions;
    delete[] this->new_jmp_instruction;
    delete[] this->saved_value;
}
