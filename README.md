# Linux-Shell
A Linux shell written in C with piping and redirect capabilities.

### Compiling
Compile on a Linux machine.
```sh
$ make
```

### Usage
Call the shell with:
```sh
$ shell [-t] [-c]
    -t Do not display prompt.
    -c Do not display colors.
```

### Syntax
```sh
$ exit
$ cd dir
$ command [< input_file] [| command] ... [> output_file] [2> output_file] [>> output_file]
    < Redirect input.
    > Redirect output (opens file with O_TRUNC).
    2> Redirect errors (opens file with O_TRUNC).
    >> Redirect output (opens file with O_APPEND).
```

### License
MIT
