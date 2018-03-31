#pragma once

class bound_imported_ref{
    std::string name;
    DWORD timestamp;
    
public:
    bound_imported_ref::bound_imported_ref();
    bound_imported_ref::bound_imported_ref(std::string name, DWORD timestamp);
    bound_imported_ref::~bound_imported_ref();

    bound_imported_ref& bound_imported_ref::operator=(const bound_imported_ref& ref);
public:
    void bound_imported_ref::set_name(std::string name);
    void bound_imported_ref::set_timestamp(DWORD timestamp);

public:
    std::string bound_imported_ref::get_name() const;
    DWORD bound_imported_ref::get_timestamp() const;
};


class bound_imported_library {
    std::string name;
    DWORD timestamp;
    std::vector<bound_imported_ref> refs;
public:
    bound_imported_library::bound_imported_library();
    bound_imported_library::~bound_imported_library();

    bound_imported_library& bound_imported_library::operator=(const bound_imported_library& lib);
public:
    void bound_imported_library::set_name(std::string name);
    void bound_imported_library::set_timestamp(DWORD timestamp);

    void bound_imported_library::add_ref(bound_imported_ref& ref);
public:
    std::string bound_imported_library::get_name() const;
    DWORD bound_imported_library::get_timestamp() const;
    unsigned int bound_imported_library::get_number_of_forwarder_refs() const;

    std::vector<bound_imported_ref>& bound_imported_library::get_refs();
};

class bound_import_table {
    std::vector<bound_imported_library> libs;
public:
    bound_import_table::bound_import_table();
    bound_import_table::~bound_import_table();

    bound_import_table& bound_import_table::operator=(const bound_import_table& imports);
public:
    void bound_import_table::add_lib(bound_imported_library& lib);
public:
    std::vector<bound_imported_library>& bound_import_table::get_libs();
};



bool get_bound_import_table(_In_ const pe_image &image, _Out_ bound_import_table& imports);
void build_bound_import_table(_In_ const pe_image &image, _Inout_ pe_section& section, 
    _In_ bound_import_table& imports);
bool erase_bound_import_table(_Inout_ pe_image &image,
    _Inout_opt_ std::vector<erased_zone>* zones = 0);					