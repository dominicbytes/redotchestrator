// Test-only extension entry point. Link this file with the extension's existing
// objects and bindings, then load c3_tests_library_init in a scratch project.
#include "common/dictionary_utils.h"
#include "common/property_utils.h"
#include "orchestration/nodes.h"
#include "orchestration/orchestration.h"

#include <godot_cpp/godot.hpp>

#define C3_CHECK(condition) ERR_FAIL_COND_V_MSG(!(condition), false, #condition)

class C3NodeTests : public RefCounted {
    GDCLASS(C3NodeTests, RefCounted);

    static Ref<Orchestration> model() {
        Ref<Orchestration> result;
        result.instantiate();
        return result;
    }

    bool test_graphs_and_functions() {
        const Ref<Orchestration> script = model();
        const Ref<OScriptGraph> source = script->create_graph("source");
        const Ref<OScriptGraph> target = script->create_graph("target");
        const Ref<OScriptNode> node = source->create_node<OScriptNodeMakeArray>({});
        source->move_node_to(node, target);
        C3_CHECK(!source->has_node(node->get_id()));
        C3_CHECK(target->has_node(node->get_id()));
        C3_CHECK(script->get_node(node->get_id()) == node);

        MethodInfo method;
        method.name = "original";
        method.return_val = PropertyUtils::make_typed("result", Variant::INT);
        const Ref<OScriptFunction> function = script->create_function(method, true);
        const Ref<OScriptGraph> graph = function->get_function_graph();
        OScriptNodeInitContext context;
        context.method = method;
        graph->create_node<OScriptNodeFunctionResult>(context);
        graph->create_node<OScriptNodeMakeArray>({});
        for (bool include_code : {false, true}) {
            const Ref<OScriptFunction> copy = script->duplicate_function("original", include_code);
            C3_CHECK(copy.is_valid() && copy != function);
            const Ref<OScriptGraph> copy_graph = copy->get_function_graph();
            C3_CHECK(copy_graph->get_functions().size() == 1);
            C3_CHECK(copy_graph->get_functions()[0] == copy);
            for (const Ref<OScriptNode>& copied : copy_graph->get_nodes()) {
                const Ref<OScriptNodeFunctionTerminator> terminator = copied;
                if (terminator.is_valid()) {
                    C3_CHECK(terminator->get_function() == copy);
                    C3_CHECK(String(terminator->get("function_id")) == copy->get_guid().to_string());
                }
            }
            copy->resize_argument_list(1);
            C3_CHECK(copy->get_owning_node()->find_pin("arg1", PD_Output).is_valid());
            C3_CHECK(function->get_argument_count() == 0);
            if (include_code) {
                C3_CHECK(copy_graph->get_nodes().size() == graph->get_nodes().size());
            }
        }
        return true;
    }

    bool test_reroutes() {
        const Ref<Orchestration> script = model();
        const Ref<OScriptGraph> graph = script->create_graph("reroutes");
        const Ref<OScriptNodeReroute> a = graph->create_node<OScriptNodeReroute>({});
        const Ref<OScriptNodeReroute> b = graph->create_node<OScriptNodeReroute>({});
        a->find_pin(0, PD_Output)->link(b->find_pin(0, PD_Input));
        C3_CHECK(!a->find_pin(0, PD_Input)->can_accept(b->find_pin(0, PD_Output)));
        b->find_pin(0, PD_Output)->link(a->find_pin(0, PD_Input));
        C3_CHECK(script->get_connections().size() == 1);
        C3_CHECK(!a->find_pin(0, PD_Input)->can_accept(a->find_pin(0, PD_Output)));

        // Persistence can contain cycles that bypass connection-time checks.
        script->set("connections", Array::make(a->get_id(), 0, b->get_id(), 0, b->get_id(), 0, a->get_id(), 0));
        a->force_reroute_type(OScriptNodeReroute::REROUTE_DATA);
        C3_CHECK(a->find_pin(0, PD_Output)->get_resolved_connections().is_empty());
        a->on_pin_connected(a->find_pin(0, PD_Input));
        a->force_reroute_type(OScriptNodeReroute::REROUTE_CONTROL);
        b->force_reroute_type(OScriptNodeReroute::REROUTE_CONTROL);
        a->on_pin_connected(a->find_pin(0, PD_Input));
        C3_CHECK(a->find_pin(0, PD_Output)->get_resolved_connections().is_empty());
        const Ref<OScriptNodeReroute> upstream = graph->create_node<OScriptNodeReroute>({});
        graph->link(upstream->get_id(), 0, a->get_id(), 0);
        upstream->on_pin_disconnected(upstream->find_pin(0, PD_Input));
        script->set("connections", Array());
        a->on_pin_disconnected(a->find_pin(0, PD_Input));
        const Ref<OScriptNodeReroute> branch = graph->create_node<OScriptNodeReroute>({});
        const Ref<OScriptNodeMakeArray> sink = graph->create_node<OScriptNodeMakeArray>({});
        sink->add_dynamic_pin();
        sink->add_dynamic_pin();
        a->find_pin(0, PD_Output)->link(b->find_pin(0, PD_Input));
        a->find_pin(0, PD_Output)->link(branch->find_pin(0, PD_Input));
        b->find_pin(0, PD_Output)->link(sink->find_pin(0, PD_Input));
        branch->find_pin(0, PD_Output)->link(sink->find_pin(1, PD_Input));
        a->on_pin_disconnected(a->find_pin(0, PD_Input));
        C3_CHECK(a->find_pin(0, PD_Output)->get_resolved_connections().size() == 2);
        return true;
    }

    bool test_arguments() {
        for (int removed : {0, 1, 2}) {
            const Ref<Orchestration> script = model();
            MethodInfo method;
            method.name = "arguments";
            for (const char* name : {"a", "b", "c"}) {
                method.arguments.push_back(PropertyUtils::make_typed(name, Variant::INT));
            }
            const Ref<OScriptFunction> function = script->create_function(method, true);
            const Ref<OScriptGraph> graph = function->get_function_graph();
            OScriptNodeInitContext context;
            context.method = method;
            const Ref<OScriptNodeCallScriptFunction> call = graph->create_node<OScriptNodeCallScriptFunction>(context);
            const Ref<OScriptNode> entry = function->get_owning_node();
            for (int i = 0; i < 3; i++) {
                call->find_pin(i + 1, PD_Input)->set_default_value(10 + i);
                graph->link(entry->get_id(), i + 1, call->get_id(), i + 1);
            }
            function->remove_argument(removed);
            C3_CHECK(script->get_connections().size() == 2);
            for (int i = 0; i < 2; i++) {
                const Ref<OScriptNodePin> pin = call->find_pin(i + 1, PD_Input);
                C3_CHECK(int(pin->get_default_value()) == 10 + i + (i >= removed));
                C3_CHECK(pin->get_connections().size() == 1);
                C3_CHECK(pin->get_connections()[0]->get_pin_name() == pin->get_pin_name());
            }
        }
        return true;
    }

    bool test_crossed_arguments() {
        for (bool reverse : {false, true}) {
            for (int removed : {0, 1}) {
                const Ref<Orchestration> script = model();
                MethodInfo method;
                method.name = "crossed";
                method.arguments.push_back(PropertyUtils::make_typed("a", Variant::INT));
                method.arguments.push_back(PropertyUtils::make_typed("b", Variant::INT));
                const Ref<OScriptFunction> function = script->create_function(method, true);
                const Ref<OScriptGraph> graph = function->get_function_graph();
                OScriptNodeInitContext context;
                context.method = method;
                const Ref<OScriptNodeCallScriptFunction> call = graph->create_node<OScriptNodeCallScriptFunction>(context);
                const Ref<OScriptNode> entry = function->get_owning_node();
                script->set("nodes", reverse ? Array::make(call, entry) : Array::make(entry, call));
                C3_CHECK(script->get_nodes()[0] == (reverse ? Ref<OScriptNode>(call) : entry));
                graph->link(entry->get_id(), 1, call->get_id(), 2);
                graph->link(entry->get_id(), 2, call->get_id(), 1);
                C3_CHECK(script->get_connections().size() == 2);

                function->remove_argument(removed);
                C3_CHECK(script->get_connections().is_empty());
                C3_CHECK(Array(script->get("connections")).is_empty());
                C3_CHECK(entry->find_pin(1, PD_Output)->get_pin_name() == StringName(removed ? "a" : "b"));
                C3_CHECK(call->find_pin(1, PD_Input)->get_pin_name() == StringName(removed ? "a" : "b"));
            }
        }
        return true;
    }

    bool test_getter_arguments() {
        for (const char* name : {"get_score", "is_score_available"}) {
            const Ref<Orchestration> script = model();
            MethodInfo method;
            method.name = name;
            const Variant::Type type = method.name == StringName("get_score") ? Variant::INT : Variant::BOOL;
            method.return_val = PropertyUtils::make_typed("result", type);
            method.arguments.push_back(PropertyUtils::make_typed("index", Variant::INT));
            const Ref<OScriptFunction> function = script->create_function(method, true);
            const Ref<OScriptGraph> graph = function->get_function_graph();
            OScriptNodeInitContext context;
            context.method = method;
            const Ref<OScriptNodeCallScriptFunction> call = graph->create_node<OScriptNodeCallScriptFunction>(context);
            const Ref<OScriptNode> entry = function->get_owning_node();
            entry->find_pin(0, PD_Output)->unlink_all();
            const Ref<OScriptNodeFunctionResult> result = graph->create_node<OScriptNodeFunctionResult>(context);
            const Ref<OScriptNodeFunctionResult> other_result = graph->create_node<OScriptNodeFunctionResult>(context);
            graph->link(entry->get_id(), 0, call->get_id(), 0);
            graph->link(entry->get_id(), 1, call->get_id(), 1);
            graph->link(call->get_id(), 0, result->get_id(), 0);
            graph->link(call->get_id(), 1, result->get_id(), 1);
            graph->link(call->get_id(), 1, other_result->get_id(), 1);
            C3_CHECK(script->get_connections().size() == 5);

            function->remove_argument(0);
            C3_CHECK(call->find_pins(PD_Input).is_empty());
            C3_CHECK(call->find_pins(PD_Output).size() == 1);
            C3_CHECK(call->is_pure());
            C3_CHECK(script->get_connections().size() == 2);
            C3_CHECK(Array(script->get("connections")).size() == 8);
            for (const OScriptConnection& connection : script->get_connections()) {
                const Ref<OScriptNodePin> from = script->get_node(connection.from_node)->find_pin(connection.from_port, PD_Output);
                const Ref<OScriptNodePin> to = script->get_node(connection.to_node)->find_pin(connection.to_port, PD_Input);
                C3_CHECK(from.is_valid() && to.is_valid());
                C3_CHECK(from->is_output() && to->is_input());
                C3_CHECK(from->is_execution() == to->is_execution());
                C3_CHECK(from->get_type() == type && to->get_type() == type);
                C3_CHECK(to->can_accept(from));
                C3_CHECK(connection.from_node == call->get_id() && connection.from_port == 0);
                C3_CHECK(from->get_pin_name() == StringName("return_value"));
            }
            for (const Ref<OScriptNodeFunctionResult>& sink : {result, other_result}) {
                const Vector<Ref<OScriptNodePin>> sources = sink->find_pin(1, PD_Input)->get_connections();
                C3_CHECK(sources.size() == 1 && sources[0] == call->find_pin(0, PD_Output));
            }
        }
        return true;
    }

    bool test_variable_copies() {
        const Ref<Orchestration> script = model();
        const Ref<OScriptVariable> array = script->create_variable("items");
        PropertyInfo info = PropertyUtils::make_typed("items", Variant::ARRAY);
        info.hint = PROPERTY_HINT_ARRAY_TYPE;
        info.hint_string = "Dictionary";
        array->set_info(info);
        array->set_default_value(Array::make(DictionaryUtils::of({{"score", 7}})));
        const Ref<OScriptVariable> copy = script->duplicate_variable("items");
        C3_CHECK(copy->get_default_value() == array->get_default_value());
        Dictionary nested = Array(copy->get_default_value())[0];
        nested["score"] = 9;
        C3_CHECK(int(Dictionary(Array(array->get_default_value())[0])["score"]) == 7);

        const Ref<OScriptVariable> dict = script->create_variable("mapping");
        info = PropertyUtils::make_typed("mapping", Variant::DICTIONARY);
        info.hint = PROPERTY_HINT_DICTIONARY_TYPE;
        info.hint_string = "String;Array";
        dict->set_info(info);
        dict->set_default_value(DictionaryUtils::of({{"scores", Array::make(7, 8)}}));
        const Ref<OScriptVariable> dict_copy = script->duplicate_variable("mapping");
        C3_CHECK(dict_copy->get_default_value() == dict->get_default_value());
        Array scores = Dictionary(dict_copy->get_default_value())["scores"];
        scores[0] = 9;
        C3_CHECK(int(Array(Dictionary(dict->get_default_value())["scores"])[0]) == 7);
        return true;
    }

    bool test_dynamic_inputs() {
        for (int removed : {0, 1, 2}) {
            const Ref<Orchestration> script = model();
            const Ref<OScriptGraph> graph = script->create_graph("collections");
            const Ref<OScriptNodeMakeArray> array = graph->create_node<OScriptNodeMakeArray>({});
            const Ref<OScriptNodeMakeDictionary> dict = graph->create_node<OScriptNodeMakeDictionary>({});
            for (int i = 0; i < 3; i++) {
                array->add_dynamic_pin();
                array->find_pin(i, PD_Input)->set_default_value(10 + i);
                dict->add_dynamic_pin();
                dict->find_pin(2 * i, PD_Input)->set_default_value(String::num_int64(i));
                dict->find_pin(2 * i + 1, PD_Input)->set_default_value(20 + i);
                graph->link(array->get_id(), 0, dict->get_id(), 2 * i + 1);
                graph->link(dict->get_id(), 0, array->get_id(), i);
            }
            array->remove_dynamic_pin(array->find_pin(removed, PD_Input));
            dict->remove_dynamic_pin(dict->find_pin(2 * removed + 1, PD_Input));
            C3_CHECK(script->get_connections().size() == 4);
            for (int i = 0; i < 2; i++) {
                C3_CHECK(array->find_pin(i, PD_Input)->get_connections().size() == 1);
                const int old = i + (i >= removed);
                C3_CHECK(int(array->find_pin(i, PD_Input)->get_default_value()) == 10 + old);
                C3_CHECK(String(dict->find_pin(2 * i, PD_Input)->get_default_value()) == String::num_int64(old));
                C3_CHECK(int(dict->find_pin(2 * i + 1, PD_Input)->get_default_value()) == 20 + old);
                C3_CHECK(dict->find_pin(2 * i + 1, PD_Input)->get_connections().size() == 1);
            }
        }
        return true;
    }

    bool test_varargs() {
        const Ref<Orchestration> script = model();
        const Ref<OScriptGraph> graph = script->create_graph("varargs");
        MethodInfo method;
        method.name = "call";
        method.flags = METHOD_FLAG_NORMAL | METHOD_FLAG_VARARG;
        method.arguments.push_back(PropertyUtils::make_typed("method", Variant::STRING_NAME));
        method.return_val = PropertyUtils::make_variant("return_value");
        OScriptNodeInitContext context;
        context.method = method;
        context.class_name = "Object";
        const Ref<OScriptNodeCallMemberFunction> call = graph->create_node<OScriptNodeCallMemberFunction>(context);
        for (int count : {0, 1, 2}) {
            C3_CHECK(!call->can_remove_dynamic_pin(call->find_pin("target", PD_Input)));
            C3_CHECK(!call->can_remove_dynamic_pin(call->find_pin("return_value", PD_Output)));
            C3_CHECK(!call->can_remove_dynamic_pin(call->find_pin("method", PD_Input)));
            call->remove_dynamic_pin(call->find_pin("target", PD_Input));
            C3_CHECK(int(call->get("variable_arg_count")) == count);
            call->add_dynamic_pin();
        }
        const Vector<Ref<OScriptNodePin>> inputs = call->find_pins(PD_Input);
        C3_CHECK(call->can_remove_dynamic_pin(inputs[inputs.size() - 1]));
        call->remove_dynamic_pin(inputs[inputs.size() - 1]);
        C3_CHECK(int(call->get("variable_arg_count")) == 2);
        return true;
    }

    bool test_optional_pins() {
        const Ref<Orchestration> script = model();
        const Ref<OScriptGraph> graph = script->create_graph("optional");
        OScriptNodeInitContext context;
        context.user_data = DictionaryUtils::of({{"with_break", true}});
        const Ref<OScriptNodeForLoop> loop = graph->create_node<OScriptNodeForLoop>(context);
        const Ref<OScriptNodeForEach> each = graph->create_node<OScriptNodeForEach>(context);
        for (const Ref<OScriptNode>& node : {Ref<OScriptNode>(loop), Ref<OScriptNode>(each)}) {
            node->find_pin("aborted", PD_Output)->link(node->find_pin("break", PD_Input));
            node->set("with_break", false);
            C3_CHECK(script->get_connections().is_empty());
            C3_CHECK(node->find_pin("break", PD_Input).is_null());
            C3_CHECK(node->find_pin("aborted", PD_Output).is_null());
            node->set("with_break", true);
            C3_CHECK(script->get_connections().is_empty());
        }
        const Ref<OScriptVariable> variable = script->create_variable("object");
        variable->set_info(PropertyUtils::make_object("object"));
        context = {};
        context.variable_name = "object";
        context.user_data = DictionaryUtils::of({{"validation", true}});
        const Ref<OScriptNodeVariableGet> get = graph->create_node<OScriptNodeVariableGet>(context);
        get->find_pin("is_invalid", PD_Output)->link(loop->find_pin("ExecIn", PD_Input));
        get->find_pin("is_valid", PD_Output)->link(each->find_pin("ExecIn", PD_Input));
        get->set_validated(false);
        C3_CHECK(script->get_connections().is_empty());
        get->set_validated(true);
        C3_CHECK(script->get_connections().is_empty());
        return true;
    }

    bool test_preload() {
        const Ref<Orchestration> script = model();
        const Ref<OScriptGraph> graph = script->create_graph("preload");
        OScriptNodeInitContext context;
        context.resource_path = "res://target.tscn";
        const Ref<OScriptNodePreload> preload = graph->create_node<OScriptNodePreload>(context);
        const Ref<OScriptNodePin> pin = preload->find_pin(0, PD_Output);
        C3_CHECK(pin->get_target_class() == StringName("PackedScene"));
        C3_CHECK(preload->resolve_type_class(pin) == StringName("PackedScene"));
        C3_CHECK(preload->resolve_target(pin)->get_target_class() == StringName("PackedScene"));
        return true;
    }

protected:
    static void _bind_methods() { ClassDB::bind_method(D_METHOD("run"), &C3NodeTests::run); }

public:
    bool run() {
        const bool crossed = test_crossed_arguments();
        const bool getters = test_getter_arguments();
        return crossed && getters && test_graphs_and_functions() && test_reroutes() && test_arguments()
            && test_variable_copies() && test_dynamic_inputs() && test_varargs()
            && test_optional_pins() && test_preload();
    }
};

static GDExtensionInitialization c3_original_initialization;

static void c3_initialize(void* p_userdata, GDExtensionInitializationLevel p_level) {
    c3_original_initialization.initialize(p_userdata, p_level);
    if (p_level == GDEXTENSION_INITIALIZATION_SCENE) {
        ClassDB::register_class<C3NodeTests>();
    }
}

extern "C" GDExtensionBool extension_library_init(GDExtensionInterfaceGetProcAddress,
    GDExtensionClassLibraryPtr, GDExtensionInitialization*);

extern "C" GDExtensionBool GDE_EXPORT c3_tests_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) {
    const GDExtensionBool result = extension_library_init(p_get_proc_address, p_library, r_initialization);
    c3_original_initialization = *r_initialization;
    r_initialization->initialize = c3_initialize;
    return result;
}
