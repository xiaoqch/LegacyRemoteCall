# Setup tests

1. Change directory to `src/test/lse/`
2. execute `npm test`, the proxy mod for tests will be generated in `bin/lrc-lse-test/`
3. Move `lrc-lse-test` to server plugins directory
4. set environment `NODE_OPTIONS` to enable typescript and start the bedrock server.

   for example, execute follow command in powershell:

```powershell
$Env:NODE_OPTIONS='--experimental-transform-types --disable-warning=ExperimentalWarning'; bedrock_server_mod.exe
```

Now, you can modify the code in `src/test/lse/src/` directly. After the modification, you need to reload the `lrc-lse-test` mod or restart the server
