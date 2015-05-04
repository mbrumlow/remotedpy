function handleMessage(message) {
    var logEl = document.getElementById('log');
    var p = document.createElement("p");
    var n = document.createTextNode(message.data);
    p.appendChild(n); 
    logEl.appendChild(p);

    logEl.scrollTop = logEl.scrollHeight;
}
