
TAG:= onixdev
SRC:= $(abspath $(shell pwd)/..)

.PHONY: build
build:
	docker build \
	--build-arg http_proxy=$(http_proxy) \
	--build-arg https_proxy=$(https_proxy) \
	-t $(TAG) \
	.

X11:= /tmp/.X11-unix/X0

$(X11):
	@echo "Please start X server as you can like startx command before run onix"
	abort

RUN_NAME:= $(TAG)

RUN_ARGS := -it
# RUN_ARGS += --rm
RUN_ARGS += --privileged
RUN_ARGS += -e USER=onix
RUN_ARGS += -e DISPLAY=unix:0
RUN_ARGS += -e XAUTHORITY=/tmp/.Xauthority
RUN_ARGS += -v /tmp/.X11-unix:/tmp/.X11-unix
RUN_ARGS += -v /dev:/dev
RUN_ARGS += --hostname $(TAG)
RUN_ARGS += -v $(SRC):/home/onix/onix
RUN_ARGS += -v ~/.vscode-server:/home/onix/.vscode-server

# RUN_ARGS += -v .:/data
RUN_ARGS += --name $(RUN_NAME)

# RUN_CMD:= /bin/bash
RUN_CMD:= /usr/bin/zsh

.PHONY:x
x:
	xhost +

.PHONY: run
run: $(X11) x
	docker run $(RUN_ARGS) $(RUN_NAME) $(RUN_CMD)

.PHONY: stop
stop:
	-docker stop $(RUN_NAME)

.PHONY: start
start: x
	@-docker start $(RUN_NAME)

.PHONY: exec
exec: x
	@-docker exec -it $(RUN_NAME) $(RUN_CMD)

.PHONY: rmc
rmc: stop
	-docker container rm $(RUN_NAME)

.PHONY: rmi
rmi: rmc
	-docker image rm $(TAG)

.PHONY: ls
ls:
	@docker image ls onix*

.PHONY: diff
diff:
	docker diff $(RUN_NAME) | more
