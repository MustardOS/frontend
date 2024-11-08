# muX

muOS Frontend


## How to compile
This [PR](https://github.com/MustardOS/frontend/pull/2) explains how to setup a local development environment.
However an alternative is to use a docker image to do the build

```
docker run --platform linux/amd64 -v .:/workspace --rm -it shengy/muos-development:latest /bin/bash
```

Note that `DEVICE` is preset to `RG35XXPLUS`, change it as needed.

See [muos-docker](https://github.com/ysheng26/muos-docker) for the `Dockerfile`.
