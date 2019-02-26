// hello.cc using N-API
#include <node_api.h>

#include "an.cpp"

Shared shared;

napi_value node_init(napi_env env, napi_callback_info info) {
    napi_status status;

    shared.init();

    return nullptr;
}

napi_value node_update(napi_env env, napi_callback_info info) {
    napi_status status;

    size_t argc = 2;
    napi_value args[2];
    assert(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) == napi_ok);

    // if (argc < 2) {
    //     napi_throw_type_error(env, nullptr, "Wrong number of arguments");
    //     return nullptr;
    // }

    napi_valuetype valuetype0;
    assert(napi_typeof(env, args[0], &valuetype0) == napi_ok);

    // napi_valuetype valuetype1;
    // assert(napi_typeof(env, args[1], &valuetype1) == napi_ok);

    // if (valuetype0 != napi_number || valuetype1 != napi_number) {
    //     napi_throw_type_error(env, nullptr, "Wrong arguments");
    //     return nullptr;
    // }

    double dt;
    status = napi_get_value_double(env, args[0], &dt);

    // double value1;
    // status = napi_get_value_double(env, args[1], &value1);

    shared.simulation_update(dt);

    shared.main_update(dt);


    size_t byte_length = sizeof(Shared::agent_instance_array);
    void * data = &(shared.agent_instance_array);
    napi_value arraybuffer;
    //status = napi_create_arraybuffer(env, byte_length, &data, &arraybuffer);
    status = napi_create_external_arraybuffer(env, data, byte_length, nullptr, nullptr, &arraybuffer);

    // napi_value sum;
    // status = napi_create_double(env, dt + value1, &sum);
    // assert(status == napi_ok);

    return arraybuffer;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

#define DECLARE_METHOD(NAME, FUNC) { \
        napi_property_descriptor descriptor = { NAME, 0, FUNC, 0, 0, 0, napi_default, 0 }; \
        assert(napi_ok == napi_define_properties(env, exports, 1, &descriptor)); \
    }

napi_value Init(napi_env env, napi_value exports) {
    napi_status status;

    DECLARE_METHOD("init", node_init)
    DECLARE_METHOD("update", node_update)

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
