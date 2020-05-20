class GstdClient {

  constructor(ip='http://localhost',port=5000){
    this.ip = ip;
    this.port = port;
  }

  list_pipelines() {
    var http = new XMLHttpRequest();
    http.onreadystatechange = alertContents;
    http.open('GET', this.ip + ":" + this.port + "/pipelines");
    http.send();
    function alertContents() {
      if (http.readyState === XMLHttpRequest.DONE) {
        if (http.status === 200) {
          console.log(http.responseText)
        } else {
          alert('There was a problem with the request.');
        }
      }
    }
  }

  pipeline_create(pipe_name, pipe_desc){
    var http = new XMLHttpRequest();
    http.onreadystatechange = alertContents;
    http.open('POST', this.ip + ":" + this.port + "/pipelines?name="+pipe_name+"&description="+pipe_desc);
    let jpipe = JSON.stringify({
      name: pipe_name,
      description: pipe_desc
    });
    http.send(jpipe);
    function alertContents() {
      if (http.readyState === XMLHttpRequest.DONE) {
        if (http.status === 200) {
          console.log(http.responseText)
        } else {
          alert('There was a problem with the request.');
        }
      }
    }
  }
  pipeline_play(pipe_name){
    var http = new XMLHttpRequest();
    http.onreadystatechange = alertContents;
    http.open('PUT', this.ip + ":" + this.port + "/pipelines/"+pipe_name+"/state?name=playing");
    let jpipe = JSON.stringify({
      name: "playing"
    });
    http.send(jpipe);
    function alertContents() {
      if (http.readyState === XMLHttpRequest.DONE) {
        if (http.status === 200) {
          console.log(http.responseText)
        } else {
          alert('There was a problem with the request.');
        }
      }
    }
  }

  element_set(pipe_name, element, prop, value){
    var http = new XMLHttpRequest();
    http.onreadystatechange = alertContents;
    http.open('PUT', this.ip + ":" + this.port + "/pipelines/"+pipe_name+"/elements/"+element+"/properties/"+prop+"?name="+value);
    let jpipe = JSON.stringify({
      name: "paused"
    });
    http.send(jpipe);
    function alertContents() {
      if (http.readyState === XMLHttpRequest.DONE) {
        if (http.status === 200) {
          console.log(http.responseText)
        } else {
          alert('There was a problem with the request.');
        }
      }
    }
   }

  pipeline_pause(pipe_name){
    var http = new XMLHttpRequest();
    http.onreadystatechange = alertContents;
    http.open('PUT', this.ip + ":" + this.port + "/pipelines/"+pipe_name+"/state?name=paused");
    let jpipe = JSON.stringify({
      name: "paused"
    });
    http.send(jpipe);
    function alertContents() {
      if (http.readyState === XMLHttpRequest.DONE) {
        if (http.status === 200) {
          console.log(http.responseText)
        } else {
          alert('There was a problem with the request.');
        }
      }
    }
  }

  pipeline_stop(pipe_name){
    var http = new XMLHttpRequest();
    http.onreadystatechange = alertContents;
    http.open('PUT', this.ip + ":" + this.port + "/pipelines/"+pipe_name+"/state?name=null");
    let jpipe = JSON.stringify({
      name: "null"
    });
    http.send(jpipe);
    function alertContents() {
      if (http.readyState === XMLHttpRequest.DONE) {
        if (http.status === 200) {
          console.log(http.responseText)
        } else {
          alert('There was a problem with the request.');
        }
      }
    }
  }

  pipeline_delete(pipe_name){
    var http = new XMLHttpRequest();
    http.onreadystatechange = alertContents;
    http.open('DELETE', this.ip + ":" + this.port + "/pipelines?name="+pipe_name);
    let jpipe = JSON.stringify({
      name: pipe_name
    });
    http.send(jpipe);
    function alertContents() {
      if (http.readyState === XMLHttpRequest.DONE) {
        if (http.status === 200) {
          console.log(http.responseText)
        } else {
          alert('There was a problem with the request.');
        }
      }
    }
  }
}