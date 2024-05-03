Containers in xv6
=======
This project implements restricted versions of Linux container primitives and provides a miniature docker-like utility to run containers on xv6.

Report link: [report](https://drive.google.com/drive/folders/1ulcNgEvHh1ETs8ws909w-nycYTQ05LBS?usp=sharing)

## Tutorial
### Running xv6
1. Clone the repo
```
git clone https://github.com/HehBot/CS695-A04
cd CS695-A04
```

2. Now xv6 can be run either natively or in a Docker container.

- To run natively:
```
sudo make run
```
- To run in a Docker container:
```
sudo make docker-build
sudo make docker-run
```

  This starts up a terminal in xv6.

3. Press `Ctrl-a + x` to exit the xv6 terminal

### Using conductor

1. Once inside xv6 terminal, first execute
```
$ conductor init
```
to setup the containers folder.

2. Now `conductor run <image> <command> [args...]` can be used to run any command inside a container based on given.
   
For example, following command runs ps command inside a contatiner based on sample image which is present by default in images folder.
```
$ conductor run sample ps
```
<img alt="Example image demonstrating conductor run" src="/demo_imgs/run_demo.png" height="170"/>

3. `conductor exec <pid> <command> [args...]` can be used to run a command inside an already running container.

For example, here a container is created which runs `container_init` which is a command that never ends in background by appending `&` at the end of command. Next it exec `ps` command into that container

<img alt="Example image demonstrating conductor exec" src="/demo_imgs/exec_demo.png" height="170"/>

4. Execute `conductor stop <pid>` to stop container and delete it's folder.

### Creating custom image

Inspect the script `CS695-A04/uprogs/prepare_sample_image.sh` and to make a similar custom script which makes new folder inside `fsroot/image` and copies required files into it. Add the custom script under `all:` in `CS695-A04/uprogs/Makefile` On running xv6 the image folder will now be present inside /image folder.

## Credits
This repo was forked from [xv6-net](https://github.com/pandax381/xv6-net).
This project was implemented as a part of Assignment-4 of CS695 Cloud computing and Virtualization course.

Contributors: [Arnav Aditya Singh](https://github.com/HehBot) (210050018), [Kevin Prafull Baua](https://github.com/Kevinbaua) (210050084)

