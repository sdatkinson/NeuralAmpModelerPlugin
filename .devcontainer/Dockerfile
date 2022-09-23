FROM emscripten/emsdk:2.0.16

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends vim faust \
    && pip3 install semver

RUN echo "alias python=python3 \n alias lr='ls -lart' \n alias cd..='cd ..'" >> "$HOME/.bashrc"
