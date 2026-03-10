FROM alpine:latest

RUN apk add --no-cache \
	build-base \
	flex \
	cmake

WORKDIR /flex_lab

COPY . .

RUN test -f /flex_lab/validator

RUN chmod +x /flex_lab/validator

ENTRYPOINT ["/flex_lab/validator"]
