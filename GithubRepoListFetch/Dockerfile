FROM archlinux:latest
WORKDIR /src

RUN pacman -Syu --noconfirm
RUN pacman -Sy --noconfirm qt6-base gcc git cmake icu make glib2
COPY . /src
RUN qmake6 GithubRepoListFetch.pro && make
RUN mkdir data

CMD ["./GithubRepoListFetch"]
