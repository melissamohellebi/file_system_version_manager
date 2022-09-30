CC= gcc

all: restore_version release_version change_version

restore: restore_version
release: release_version
change: change_version

restore_version: restore_version.c
	$(CC) -o $@  $<

release_version: release_version.c
	$(CC) -o $@  $<

change_version: change_version.c
	$(CC) -o $@  $<

clean:
	-rm -f restore_version release_version change_version

.PHONY: all clean
