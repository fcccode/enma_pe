#include "stdafx.h"
#include "pe_section_io.h"


pe_section_io::pe_section_io(pe_section & _section,
    section_io_mode mode,
    section_io_addressing_type type,
    uint32_t raw_aligment,
    uint32_t virtual_aligment) :
    section(&_section), section_offset(0),
    last_code(section_io_success), mode(mode), addressing_type(type), 
    raw_aligment(raw_aligment), virtual_aligment(virtual_aligment){}

pe_section_io::pe_section_io(const pe_section_io& io_section) {
    this->operator=(io_section);
}

pe_section_io::~pe_section_io() {

}

pe_section_io& pe_section_io::operator=(const pe_section_io& io_section) {
    this->section           = io_section.section;
    this->section_offset    = io_section.section_offset;
    this->last_code         = io_section.last_code;
    this->mode              = io_section.mode;
    this->addressing_type   = io_section.addressing_type;

    return *this;
}

bool pe_section_io::view_data(
    uint32_t  required_offset, uint32_t required_size,uint32_t& real_offset,
    uint32_t& available_size, uint32_t& down_oversize, uint32_t& up_oversize,
    uint32_t present_offset, uint32_t present_size) {


    //         ...............
    //  .............................
    //  |    | |             |      |
    //  v    v |             |      |
    // (down_oversize)       |      |
    //         |             |      |
    //         v             v      |
    //         (available_size)     |
    //                       |      |
    //                       v      v
    //                       (up_oversize)

    real_offset     = 0;
    available_size  = 0;
    down_oversize   = 0;
    up_oversize     = 0;

    if (required_offset < present_offset) {
        down_oversize = (present_offset - required_offset);

        if (down_oversize >= required_size) {

            return false; //not in bounds
        }
        else {
            available_size = (required_size - down_oversize);

            if (available_size > present_size) {
                up_oversize = (available_size - present_size);
                available_size = present_size;

                return true;//partially in bounds
            }

            return true;//partially in bounds
        }
    }
    else {//if(required_offset >= present_offset)

        if (required_offset < (present_offset + present_size)) {
            real_offset = (required_offset - present_offset);

            if (required_size + required_offset > (present_offset + present_size)) {
                up_oversize = (required_size + required_offset) - (present_offset + present_size);
                available_size = required_size - up_oversize;

                return true; //partially in bounds
            }
            else {
                available_size = required_size;

                return true; //full in bounds
            }
        }
        else {
            real_offset = (required_offset - present_offset + present_size);
            up_oversize = (required_size + required_offset) - (present_offset + present_size);
            available_size = 0;

            return true; //trough full adding size 
        }
    }

    return true;
}

bool pe_section_io::view_section(
    uint32_t required_offset, uint32_t required_size,uint32_t& real_offset,
    uint32_t& available_size, uint32_t& down_oversize, uint32_t& up_oversize) {

 
    switch (addressing_type) {
        case section_io_addressing_type::section_address_raw: {
            return view_data(
                required_offset, required_size,
                real_offset, available_size, down_oversize, up_oversize,
                this->section->get_pointer_to_raw(), ALIGN_UP(section->get_size_of_raw_data(), this->raw_aligment)
            );
        }

        case section_io_addressing_type::section_address_rva: {
            return view_data(
                required_offset, required_size,
                real_offset, available_size, down_oversize, up_oversize,
                this->section->get_virtual_address(), ALIGN_UP(this->section->get_virtual_size(), this->virtual_aligment)
            );
        }
    }
    
    return false;
}

uint32_t pe_section_io::get_present_size(uint32_t required_offset) {

    if (section->get_pointer_to_raw() > required_offset) {
        return section->get_size_of_raw_data();
    }
    else {
        if (section->get_size_of_raw_data() > required_offset) {
            return section->get_size_of_raw_data() - required_offset;
        }
        else {
            return 0;
        }
    }

    return 0;
}

section_io_code pe_section_io::internal_read(
    uint32_t data_offset,
    void * buffer, uint32_t size,
    uint32_t& readed_size, uint32_t& down_oversize, uint32_t& up_oversize) {

    uint32_t real_offset    = 0;

    bool b_view = view_section(data_offset, size,
        real_offset,
        readed_size, down_oversize, up_oversize);


    if (b_view && readed_size) {
        uint32_t present_size = get_present_size(real_offset);

        if (readed_size > present_size) {
            if (present_size) {
                memcpy(&((uint8_t*)buffer)[down_oversize], &section->get_section_data().data()[real_offset], present_size);
            }
            memset(&((uint8_t*)buffer)[down_oversize + present_size], 0, readed_size - present_size);
        }
        else {
            memcpy(&((uint8_t*)buffer)[down_oversize], &section->get_section_data().data()[real_offset], readed_size);
        }

        if (down_oversize || up_oversize) {

            last_code = section_io_code::section_io_incomplete;
            return section_io_code::section_io_incomplete;
        }

        last_code = section_io_code::section_io_success;
        return section_io_code::section_io_success;
    }

    last_code = section_io_code::section_io_data_not_present;
    return section_io_code::section_io_data_not_present;
}

section_io_code pe_section_io::internal_write(
    uint32_t data_offset,
    void * buffer, uint32_t size,
    uint32_t& wrote_size, uint32_t& down_oversize, uint32_t& up_oversize) {


    uint32_t real_offset = 0;

    bool b_view = view_section(data_offset, size,
        real_offset,
        wrote_size, down_oversize, up_oversize);


    if (b_view &&
        (wrote_size || (up_oversize && mode == section_io_mode::section_io_mode_allow_expand) )) {

        if ((up_oversize && mode == section_io_mode::section_io_mode_allow_expand)) { 
            if (addressing_type == section_io_addressing_type::section_address_raw) {

                add_size(
                    (ALIGN_UP(section->get_size_of_raw_data(), this->raw_aligment) - section->get_size_of_raw_data()) + up_oversize            
                    , false);

                wrote_size += up_oversize;
                up_oversize = 0;
            }
            else if (addressing_type == section_io_addressing_type::section_address_rva) {

                add_size(
                    (ALIGN_UP(section->get_virtual_size(), this->virtual_aligment) - section->get_virtual_size()) + up_oversize
                    , false);

                wrote_size += up_oversize;
                up_oversize = 0;
            }
            else {
                return section_io_code::section_io_data_not_present;//unk addressing_type
            }        
        }
        else { //check if necessary add an emulated part
            if (addressing_type == section_io_addressing_type::section_address_raw) {
                if (real_offset + wrote_size > section->get_size_of_raw_data()) {

                    add_size((real_offset + wrote_size) - section->get_size_of_raw_data(), false);
                }
            }
            else if (addressing_type == section_io_addressing_type::section_address_rva) {
                if (real_offset + wrote_size > section->get_virtual_size()) {

                    add_size((real_offset + wrote_size) - section->get_virtual_size(), false);
                }
            }
        }

        memcpy(&section->get_section_data().data()[real_offset], &((uint8_t*)buffer)[down_oversize], wrote_size);

        if (down_oversize || up_oversize) {

            last_code = section_io_code::section_io_incomplete;
            return section_io_code::section_io_incomplete;
        }

        last_code = section_io_code::section_io_success;
        return section_io_code::section_io_success;
    }

    last_code = section_io_code::section_io_data_not_present;
    return section_io_code::section_io_data_not_present;
}

section_io_code pe_section_io::read(void * data, uint32_t size) {

    uint32_t readed_size;
    uint32_t down_oversize;
    uint32_t up_oversize;


    section_io_code code = internal_read(section_offset, data, size,
        readed_size, down_oversize, up_oversize);

    section_offset += readed_size;

    return code;
}

section_io_code pe_section_io::write(void * data, uint32_t size) {

    uint32_t wrote_size;
    uint32_t down_oversize;
    uint32_t up_oversize;

    section_io_code code = internal_write(section_offset, data, size,
        wrote_size, down_oversize, up_oversize);

    section_offset += wrote_size;

    return code;
}

section_io_code pe_section_io::read(std::vector<uint8_t>& buffer, uint32_t size) {

    if (buffer.size() < size) { buffer.resize(size); }

    return read(buffer.data(), buffer.size());
}

section_io_code pe_section_io::write(std::vector<uint8_t>& buffer) {

    return write(buffer.data(), buffer.size());
}

pe_section_io& pe_section_io::align_up(uint32_t factor, bool offset_to_end) {
    section->get_section_data().resize(ALIGN_UP(section->get_size_of_raw_data(), factor));
    if (offset_to_end) {
        section_offset = section->get_size_of_raw_data();
    }
    add_size(ALIGN_UP(section->get_size_of_raw_data(), factor));

    return *this;
}

pe_section_io& pe_section_io::add_size(uint32_t size, bool offset_to_end) {
    if (size) {
        section->get_section_data().resize(section->get_size_of_raw_data() + size);
    }
    if (offset_to_end) {
        section_offset = section->get_size_of_raw_data();
    }

    if (section->get_size_of_raw_data() > section->get_virtual_size()) {
        section->set_virtual_size(section->get_size_of_raw_data());
    }


    return *this;
}

pe_section_io& pe_section_io::set_mode(section_io_mode mode) {
    this->mode = mode;

    return *this;
}
pe_section_io& pe_section_io::set_addressing_type(section_io_addressing_type type) {
    this->addressing_type = type;

    return *this;
}
pe_section_io& pe_section_io::set_section_offset(uint32_t offset) {
    this->section_offset = offset;

    return *this;
}
pe_section_io& pe_section_io::set_raw_aligment(uint32_t aligment) {
    this->raw_aligment = aligment;

    return *this;
}
pe_section_io& pe_section_io::set_virtual_aligment(uint32_t aligment) {
    this->virtual_aligment = aligment;

    return *this;
}


section_io_mode pe_section_io::get_mode() const {
    return this->mode;
}
section_io_code pe_section_io::get_last_code() const {
    return this->last_code;
}
section_io_addressing_type pe_section_io::get_addressing_type() const {
    return this->addressing_type;
}
uint32_t pe_section_io::get_section_offset() const {
    return this->section_offset;
}
uint32_t pe_section_io::get_raw_aligment() const {
    return this->raw_aligment;
}
uint32_t pe_section_io::get_virtual_aligment() const {
    return this->virtual_aligment;
}
pe_section* pe_section_io::get_section() {
    return this->section;
}
