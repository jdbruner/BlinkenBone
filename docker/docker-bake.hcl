target "default" {
  name = "${pidp}"
  matrix = {
    pidp = ["pidp11","pidp10"]
  }
  dockerfile = "Dockerfile"
  args = {
    PIDP = pidp
    GIT_REPO="https://github.com/jdbruner/simh"
  }
  tags = ["${pidp}"]
}
