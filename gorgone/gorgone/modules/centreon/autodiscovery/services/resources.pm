# 
# Copyright 2019 Centreon (http://www.centreon.com/)
#
# Centreon is a full-fledged industry-strength solution that meets
# the needs in IT infrastructure and application monitoring for
# service performance.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

package gorgone::modules::centreon::autodiscovery::services::resources;

use strict;
use warnings;

sub get_pollers {
    my (%options) = @_;
    
    my ($status, $pollers) = $options{class_object_centreon}->custom_execute(
        request => 'SELECT id, name FROM nagios_server',
        mode => 1,
        keys => 'id'
    );
    if ($status == -1) {
        return (-1, 'cannot get poller list');
    }

    if (scalar(keys %$pollers) == 0) {
        return (-1, 'no pollers found in configuration');
    }

    foreach my $poller_id (keys %$pollers) {
        $pollers->{$poller_id}->{resources} = {};
        ($status, my $resources) = $options{class_object_centreon}->custom_execute(
            request =>
                'SELECT resource_name, resource_line FROM cfg_resource_instance_relations, cfg_resource WHERE cfg_resource_instance_relations.instance_id = ' .
                $options{class_object_centreon}->quote(value => $poller_id) . "AND cfg_resource_instance_relations.resource_id = cfg_resource.resource_id AND resource_activate = '1'",
            mode => 2
        );
        if ($status == -1) {
            return (-1, 'cannot get rules resource list');
        }

        foreach (@$resources) {
            $pollers->{$poller_id}->{resources}->{ $_->[0] } = $_->[1];
        }
    }

    return (0, '', $pollers);
}

sub get_audit_user_id {
    my (%options) = @_;
    my $user_id = 0;

    my ($status, $contacts) = $options{class_object_centreon}->custom_execute(
        request =>
            'SELECT contact_id FROM contact WHERE contact_alias = ' .
            $options{class_object_centreon}->quote(value => $options{clapi_user}),
        mode => 2
    );
    if ($status == -1) {
        return (-1, 'cannot get audit user');
    }

    if (defined($contacts->[0])) {
        $user_id = $contacts->[0]->[0];
    }

    return (0, '', $user_id);
}

sub get_rules {
    my (%options) = @_;
    
    my $filter = "rule_activate = '1' AND ";
    if (defined($options{force_rule})) {
        $filter = '';
    }

    my ($status, $rules) = $options{class_object_centreon}->custom_execute(
        request =>
            "SELECT rule_id, rule_alias, service_display_name, rule_disable, rule_update, command_line, service_template_model_id, rule_scan_display_custom, rule_variable_custom
              FROM mod_auto_disco_rule, command WHERE " . $filter . " mod_auto_disco_rule.command_command_id = command.command_id",
        mode => 1,
        keys => 'rule_id'
    );
    if ($status == -1) {
        return (-1, 'cannot get rules list');
    }
    if (scalar(keys %$rules) == 0) {
        return (-1, 'no rules found in configuration');
    }
    
    $filter = '(' . join(',', keys %$rules) . ')';
    
    ############################
    # Get mod_auto_disco_change
    ($status, my $datas) = $options{class_object_centreon}->custom_execute(
        request => 'SELECT rule_id, change_str, change_regexp, change_replace, change_modifier FROM mod_auto_disco_change WHERE rule_id IN ' . $filter . ' ORDER BY rule_id, change_order ASC',
        mode => 2
    );
    if ($status == -1) {
        return (-1, 'cannot get rules change list');
    }
    foreach (@$datas) {
        $rules->{ $_->[0] }->{change} = [] if (!defined($rules->{ $_->[0] }->{change}));
        push @{$rules->{ $_->[0] }->{change}}, { change_str => $_->[1], change_regexp => $_->[2], change_replace => $_->[3], change_modifier => $_->[4] };
    }
    
    #########################################
    # Get mod_auto_disco_inclusion_exclusion
    ($status, $datas) = $options{class_object_centreon}->custom_execute(
        request => 'SELECT rule_id, exinc_type, exinc_str, exinc_regexp FROM mod_auto_disco_inclusion_exclusion WHERE rule_id IN ' . $filter . ' ORDER BY rule_id, exinc_order ASC',
        mode => 2
    );
    if ($status == -1) {
        return (-1, 'cannot get rules exinc list');
    }
    foreach (@$datas) {
        $rules->{ $_->[0] }->{exinc} = [] if (!defined($rules->{ $_->[0] }->{exinc}));
        push @{$rules->{ $_->[0] }->{exinc}}, { exinc_type => $_->[1], exinc_str => $_->[2], exinc_regexp => $_->[3] };
    }
    
    #########################################
    # Get mod_auto_disco_macro
    ($status, $datas) = $options{class_object_centreon}->custom_execute(
        request => 'SELECT rule_id, macro_name, macro_value, is_empty FROM mod_auto_disco_macro WHERE rule_id IN ' . $filter,
        mode => 2
    );
    if ($status == -1) {
        return (-1, 'cannot get rules macro list');
    }
    foreach (@$datas) {
        $rules->{ $_->[0] }->{macro} = {} if (!defined($rules->{ $_->[0] }->{macro}));
        $rules->{ $_->[0] }->{macro}->{ $_->[1] } = { macro_value => $_->[2], is_empty => $_->[3] };
    }
    
    #########################################
    # Get mod_auto_disco_inst_rule_relation
    ($status, $datas) = $options{class_object_centreon}->custom_execute(
        request => 'SELECT rule_rule_id as rule_id, instance_id FROM mod_auto_disco_inst_rule_relation WHERE rule_rule_id IN ' . $filter,
        mode => 2
    );
    if ($status == -1) {
        return (-1, 'cannot get rules instance list');
    }
    foreach (@$datas) {
        $rules->{ $_->[0] }->{poller_id} = [] if (!defined($rules->{ $_->[0] }->{poller_id}));
        push @{$rules->{ $_->[0] }->{poller_id}}, $_->[1];
    }

    #########################################
    # Get mod_auto_disco_ht_rule_relation
    ($status, $datas) = $options{class_object_centreon}->custom_execute(
        request => 'SELECT rule_rule_id as rule_id, host_host_id FROM mod_auto_disco_ht_rule_relation WHERE rule_rule_id IN ' . $filter,
        mode => 2
    );
    if ($status == -1) {
        return (-1, 'cannot get rules host template list');
    }
    foreach (@$datas) {
        $rules->{ $_->[0] }->{host_template} = [] if (!defined($rules->{ $_->[0] }->{host_template}));
        push @{$rules->{ $_->[0] }->{host_template}}, $_->[1];
    }
    
    ########################################
    # Get services added by autodisco
    ($status, $datas) = $options{class_object_centreon}->custom_execute(
        request => 'SELECT rule_rule_id as rule_id, host_host_id as host_id, service_id, service_activate, service_description FROM mod_auto_disco_rule_service_relation, service, host_service_relation WHERE rule_rule_id IN ' . $filter . " AND mod_auto_disco_rule_service_relation.service_service_id = service.service_id AND service.service_id = host_service_relation.service_service_id",
        mode => 2
    );
    if ($status == -1) {
        return (-1, 'cannot get rules host template list');
    }
    foreach (@$datas) {
        $rules->{ $_->[0] }->{linked_services} = {} if (!defined($rules->{ $_->[0] }->{linked_services}));
        $rules->{ $_->[0] }->{linked_services}->{ $_->[1] } = {} if (!defined($rules->{ $_->[0] }->{linked_services}->{ $_->[1] }));
        $rules->{ $_->[0] }->{linked_services}->{ $_->[1] }->{ $_->[2] } = {
            service_activate => $_->[3], service_description => $_->[3]
        };
    }
    
    #########################################
    # Get Contact
    ($status, $datas) = $options{class_object_centreon}->custom_execute(
        request => 'SELECT rule_id, contact_id, cg_id FROM mod_auto_disco_rule_contact_relation WHERE rule_id IN ' . $filter,
        mode => 2
    );
    if ($status == -1) {
        return (-1, 'cannot get rules contact list');
    }
    foreach (@$datas) {
        if (defined($_->[1])) {
            # Already add it
            next if (defined($rules->{ $_->[0] }->{contact}->{ $_->[1] }));
            if ((my $contact = get_contact(class_object_centreon => $options{class_object_centreon}, contact_id => $_->[1]))) {
                $rules->{ $_->[0] }->{contact} = {} if (!defined($rules->{ $_->[0] }->{contact}));
                $rules->{ $_->[0] }->{contact}->{ $contact->{contact_id} } = { contact_email => $contact->{contact_email} };
            }
        } elsif (defined($_->[2])) {
            ($status, my $datas2) = $options{class_object_centreon}->custom_execute(
                request => "SELECT contact_contact_id as contact_id FROM contactgroup, contactgroup_contact_relation WHERE contactgroup.cg_id = '" . $_->[2] . "' AND contactgroup.cg_id = contactgroup_contact_relation.contactgroup_cg_id",
                mode => 2
            );
            if ($status == -1) {
                return (-1, 'cannot get rules contactgroup list');
            }
            foreach my $row (@$datas2) {
                # Already add it
                next if (defined($rules->{ $_->[0] }->{contact}->{ $row->[1] }));
                if ((my $contact = get_contact(class_object_centreon => $options{class_object_centreon}, contact_id => $row->[1]))) {
                    $rules->{ $_->[0] }->{contact} = {} if (!defined($rules->{ $_->[0] }->{contact}));
                    $rules->{ $_->[0] }->{contact}->{$contact->{contact_id}} = { contact_email => $contact->{contact_email} };
                }
            }
        }
    }

    # Filter rules
    if (defined($options{filter_rules}) && ref($options{filter_rules}) eq 'SCALAR') {
        foreach (keys %$rules) {
            my $find = 0;
            foreach my $opt_rule (@{$options{filter_rules}}) {
                if ($opt_rule eq $rules->{$_}->{rule_alias}) {
                    $find = 1;
                    last;
                }
            }
            
            if ($find == 0) {
                delete $rules->{$_};
            }
        }
    }

    return (0, '', $rules);
}

sub get_contact {
    my (%options) = @_;

    my ($status, $datas) = $options{class_object_centreon}->custom_execute(
        request => "SELECT contact_id, contact_email FROM contact WHERE contact_id = '" . $options{contact_id} . "' AND contact_activate = '1'",
        mode => 1
    );

    if ($status == -1) {
        return 0;
    }

    return defined($datas->[0]) ? $datas->[0] : undef;
}

my $done_macro_host = {};

sub reset_macro_hosts {
    $done_macro_host = {};
}

sub get_hosts {
    my (%options) = @_;

    if (!defined($options{host_template}) || scalar(@{$options{host_template}}) == 0) {
        return (0, 'cannot get host list', []);
    }

    my $filter = '';
    my $filter_append = '';
    foreach (@{$options{host_template}}) {
        $filter .= $filter_append . $options{class_object_centreon}->quote(value => $_);
        $filter_append = ', ';
    }
    $filter = ' host_template_relation.host_tpl_id IN (' . $filter . ') AND ';

    my $filter_host = '';
    if (defined($options{host_lookup}) && ref($options{host_lookup}) eq 'ARRAY' && scalar(@{$options{host_lookup}}) > 0) {
        my $filter_append = '';
        foreach (@{$options{host_lookup}}) {
            $filter_host .= $filter_append .$options{class_object_centreon}->quote(value => $_);
            $filter_append = ', ';
        }
        $filter_host = ' host.host_name IN (' . $filter_host . ') AND ';
    }
    
    my $filter_poller = '';
    my $join_table = '';
    if (defined($options{poller_lookup}) && ref($options{host_lookup}) eq 'ARRAY' && scalar(@{$options{poller_lookup}}) > 0) {
        my $filter_append = '';
        foreach (@{$options{poller_lookup}}) {
            $filter_poller .= $filter_append . $options{class_object_centreon}->quote(value => $_);
            $filter_append = ', ';
        }
        $filter_poller = ' nagios_server.name IN ('. $filter_poller .') AND nagios_server.id = ns_host_relation.nagios_server_id AND ';
        $join_table = ', nagios_server ';
    } elsif (defined($options{poller_id}) && scalar(@{$options{poller_id}}) > 0){
        my $filter_append = '';
        foreach (@{$options{poller_id}}) {
            $filter_poller .= $filter_append . $options{class_object_centreon}->quote(value => $_);
            $filter_append = ', ';
        }
        $filter_poller =' ns_host_relation.nagios_server_id IN (' . $filter_poller . ') AND nagios_server.id = ns_host_relation.nagios_server_id AND ';
        $join_table = ', nagios_server ';
    }

    my ($status, $datas) = $options{class_object_centreon}->custom_execute(
        request => "SELECT host_id, host_address, host_name, nagios_server_id as poller_id
            FROM host_template_relation, host, ns_host_relation " . $join_table . "
            WHERE " . $filter_host . $filter . " host_template_relation.host_host_id = host.host_id
            AND " . $filter_poller . " host.host_id = ns_host_relation.host_host_id
            AND `host_activate` = '1'
            ",
        mode => 1,
        keys => 'host_id',
    );
    if ($status == -1) {
        return (-1, 'cannot host list');
    }

    my $hosts = {};
    my $count = 0;
    foreach my $host_id (keys %$datas) {
        if (defined($options{with_macro}) && $options{with_macro} == 1) {
            if (defined($done_macro_host->{ $host_id })) {
                $datas->{$host_id}->{macros} = $done_macro_host->{ $host_id };
            } else {
                ($status, my $message, my $macros) = get_macros_host(host_id => $host_id, class_object_centreon => $options{class_object_centreon});
                if ($status == -1) {
                    return (-1, $message);
                }
                $datas->{$host_id}->{macros} = $macros;
                $done_macro_host->{ $host_id } = $macros;
            }
        }

        $count++;
        push @{$hosts->{ $datas->{$host_id}->{poller_id} }}, $datas->{$host_id};
    }

    return (0, '', $hosts, $count);
}

sub set_macro {
    my ($macros, $name, $value) = @_;
    
    if (!defined($macros->{$name})) {
        $macros->{$name} = $value;
    }
}

sub get_macros_host {
    my (%options) = @_;
    my ($status, $datas);
    my %macros = ();
    my %loop_stop = ();
    my @stack = ($options{host_id});
    
    while ((my $lhost_id = shift(@stack))) {
        if (defined($loop_stop{$lhost_id})) {
            # Already done the host
            next;
        }
        $loop_stop{$lhost_id} = 1;

        ($status, $datas) = $options{class_object_centreon}->custom_execute(
            request => "SELECT host_snmp_community, host_snmp_version FROM host WHERE host_id = " . $lhost_id . " LIMIT 1",
            mode => 2
        );
        if ($status == -1) {
            return (-1, 'get macro: cannot get snmp information');
        }

        if (defined($datas->[0]->[0]) && $datas->[0]->[0] ne "") {
            set_macro(\%macros, '$_HOSTSNMPCOMMUNITY$', $datas->[0]->[0]);
        }
        if (defined($datas->[0]->[1]) && $datas->[0]->[1] ne "") {
            set_macro(\%macros, '$_HOSTSNMPVERSION$', $datas->[0]->[1]);
        }

        ($status, $datas) = $options{class_object_centreon}->custom_execute(
            request => "SELECT host_macro_name, host_macro_value FROM on_demand_macro_host WHERE host_host_id = " . $lhost_id,
            mode => 2
        );
        if ($status == -1) {
            return (-1, 'get macro: cannot get on_demand_macro_host');
        }
        foreach (@$datas) {
            set_macro(\%macros, $_->[0], $_->[1]);
        }

        ($status, $datas) = $options{class_object_centreon}->custom_execute(
            request => "SELECT host_tpl_id FROM host_template_relation WHERE host_host_id = " . $lhost_id . " ORDER BY `order` DESC",
            mode => 2
        );
        if ($status == -1) {
            return (-1, 'get macro: cannot get host_template_relation');
        }
        foreach (@$datas) {
            unshift @stack, $_->[0];
        }
    }

    return \%macros;
}

1;
