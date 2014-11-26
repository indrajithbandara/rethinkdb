// Copyright 2010-2014 RethinkDB, all rights reserved.
#ifndef CLUSTERING_ADMINISTRATION_TABLES_TABLE_CONFIG_HPP_
#define CLUSTERING_ADMINISTRATION_TABLES_TABLE_CONFIG_HPP_

#include <string>
#include <vector>

#include "errors.hpp"
#include <boost/shared_ptr.hpp>

#include "clustering/administration/servers/config_client.hpp"
#include "clustering/administration/tables/table_common.hpp"
#include "containers/uuid.hpp"
#include "rdb_protocol/artificial_table/backend.hpp"
#include "rpc/semilattice/view.hpp"

class real_reql_cluster_interface_t;

/* This is publicly exposed so that it can be used to create the return value of
`table.reconfigure()`. */
ql::datum_t convert_table_config_to_datum(
        const table_config_t &config,
        admin_identifier_format_t identifier_format,
        server_config_client_t *server_config_client);

class table_config_artificial_table_backend_t :
    public common_table_artificial_table_backend_t
{
public:
    table_config_artificial_table_backend_t(
            boost::shared_ptr< semilattice_readwrite_view_t<
                cluster_semilattice_metadata_t> > _semilattice_view,
            real_reql_cluster_interface_t *_reql_cluster_interface,
            admin_identifier_format_t _identifier_format,
            server_config_client_t *_server_config_client) :
        common_table_artificial_table_backend_t(_semilattice_view, _identifier_format),
        reql_cluster_interface(_reql_cluster_interface),
        server_config_client(_server_config_client)
        { }

    bool write_row(
            ql::datum_t primary_key,
            bool pkey_was_autogenerated,
            ql::datum_t *new_value_inout,
            signal_t *interruptor,
            std::string *error_out);

private:
    bool format_row(
            namespace_id_t table_id,
            name_string_t table_name,
            const ql::datum_t &db_name_or_uuid,
            const namespace_semilattice_metadata_t &metadata,
            signal_t *interruptor,
            ql::datum_t *row_out,
            std::string *error_out);

    real_reql_cluster_interface_t *reql_cluster_interface;
    server_config_client_t *server_config_client;
};

#endif /* CLUSTERING_ADMINISTRATION_TABLES_TABLE_CONFIG_HPP_ */

