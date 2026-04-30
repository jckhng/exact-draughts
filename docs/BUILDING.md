# Building

Build the ARM Kindle binary and extension package with:

```bash
./docker_rebuild.sh
```

The script starts the persistent `kindle-draughts-armhf-builder` container,
builds `kindle-draughts`, runs `smoke-test`, and writes:

```text
dist/kindle-draughts-extension.zip
```
