FROM alpine:latest
WORKDIR /server  
COPY . ./
RUN apk add make build-base && \
	make release FLAGS=-DDOCKER -j `nproc` && \
	apk --purge del make build-base && \
	rm -rf `ls | grep -v "server\|./\|../"`
EXPOSE 4080
CMD ["./server"]
