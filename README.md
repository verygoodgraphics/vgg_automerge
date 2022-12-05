# VGG Automerge

A C++ implementation of [automerge](https://github.com/automerge/automerge-rs), at an early stage.
Updated to [commit](https://github.com/automerge/automerge-rs/commit/d28767e689977862dd0f214f75e4383d27540561).

Some components are under working listed as below:
- text data type
- history related APIs
- OpObserver
- sync methods
- exceptions
- test cases
- benchmark

## Advantage

Added several new APIs, users can operate an Automerge doc like a json object, which supported by [nlohmann::json](https://github.com/nlohmann/json).

``` C++
json json_obj = json::parse(R"({
"todo": [
    {
        "title": "work A",
        "done": false
    },
    {
        "title": "work B",
        "done": true
    }
]
})");

// from_json
Automerge automerge_doc = json_obj.get<Automerge>();

// to_json
json new_json_obj = automerge_doc;

assert(new_json_obj == json_obj);

// json_add
json work_C = { {"title", "work C"}, {"done", false} };
automerge_doc.json_add("/todo/0"_json_pointer, work_C);
automerge_doc.commit();

// json_replace
automerge_doc.json_replace("/todo/1/done"_json_pointer, true);
automerge_doc.commit();

// json_delete
automerge_doc.json_delete("/todo/2"_json_pointer);
automerge_doc.commit();

assert(json(automerge_doc) == json::parse(R"({
"todo": [
    {
        "title": "work C",
        "done": false
    },
    {
        "title": "work A",
        "done": true
    }
]
})"));
```

## Usage

- list

  `cmake --list-presets=all .`

- configure

  `cmake --preset <configurePreset>`, where `<configurePreset>` is the name of the active Configure Preset.

- build

  `cmake --build out/build/<configurePreset>`

  or `cmake --build --preset <buildPreset>`, where `<buildPreset>` is the name of the active Build Preset.

## Thanks
Thanks to [automerge](https://github.com/automerge/automerge-rs) contributors' awesome work.
