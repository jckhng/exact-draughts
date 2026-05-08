# Building

Build the ARM Kindle binary and extension package with:

```bash
./docker_rebuild.sh
```

The script starts the persistent `exact-draughts-armhf-builder` container,
builds `exact-draughts`, runs `smoke-test`, and writes:

```text
dist/exact-draughts-extension.zip
```
