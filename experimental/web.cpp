#include "web.h"

Web web; // Vytvoření instance třídy Web

void Web::start() {
    server.begin(); // Spuštění webového serveru na portu 80
}

// Webové stránky 

    // Menu
        const char menuPage[] PROGMEM = R"rawliteral(
            <!DOCTYPE html>
            <html lang="cs">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <title>Demonstrační firmware pro ESP32</title>
                <style>
                body {
                    font-family: Arial, sans-serif;
                    background-color: #222;
                    color: #fff;
                    margin: 0;
                    padding: 0;
                }
                header {
                    background-color: #444;
                    padding: 15px;
                    text-align: center;
                    font-size: 1.5em;
                }
                nav {
                    display: flex;
                    flex-direction: column;
                    padding: 0;
                    margin: 0;
                    list-style: none;
                }
                nav a {
                    display: block;
                    padding: 15px;
                    margin: 5px 10px;
                    background-color: #555;
                    color: #fff;
                    text-decoration: none;
                    border-radius: 8px;
                    text-align: center;
                    font-size: 1.2em;
                    transition: background 0.3s;
                }
                nav a:hover {
                    background-color: #ff6600;
                }
                footer {
                    text-align: center;
                    padding: 10px;
                    font-size: 0.9em;
                    color: #aaa;
                }
                </style>
            </head>
            <body>
                <header>Hlavní menu</header>
                <nav>
                    <a href="#" id="ppgLink">PPG</a>
                    <a href="#" id="levelLink">Gyroskop</a>
                    <a href="#" id="snakeLink">Had</a>
                    <a href="#" id="rgbLink">Ovládání barevné LED</a>
                </nav>
                <footer>Demonstrační firmware pro ESP32</footer>

                <script>
                // Přepnutí na RGB přes web
                document.getElementById("rgbLink").addEventListener("click", function(e){
                    e.preventDefault(); 
                    fetch('/rgb/activate')
                        .then(resp => window.location.href = '/rgb'); 
                });

                // Přepnutí na Snake přes web
                document.getElementById("snakeLink").addEventListener("click", function(e){
                    e.preventDefault();
                    fetch('/snake/activate') // zavolá endpoint pro hada
                        .then(resp => window.location.href = '/snake'); 
                });

                // Přepnutí na Level přes web
                document.getElementById("levelLink").addEventListener("click", function(e){
                    e.preventDefault();
                    fetch('/level/activate') // zavolá endpoint pro level
                        .then(resp => window.location.href = '/level');
                });

                // Přepnutí na PPG přes web
                document.getElementById("ppgLink").addEventListener("click", function(e){
                    e.preventDefault();
                    fetch('/ppg/activate') // zavolá endpoint pro level
                        .then(resp => window.location.href = '/ppg');
                });
                </script>
            </body>
            </html>
        )rawliteral";


    // RGB Menu
        const char rgbPage[] PROGMEM = R"rawliteral(
            <!DOCTYPE html>
            <html lang="cs">
            <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <title>Demonstrační firmware pro ESP32</title>

            <style>
            body{font-family:Arial,sans-serif;background:#222;color:#fff;margin:0;padding:0;}
            header{background:#444;padding:15px;text-align:center;font-size:1.5em;}
            .slider-container{margin:15px 10px;}
            label{display:block;margin-bottom:4px;}
            input[type=range]{width:100%;height:20px;border-radius:5px;background:#555;}
            #r{accent-color:red;}
            #g{accent-color:green;}
            #b{accent-color:blue;}

            .color-display{width:100px;height:50px;margin:10px auto;border-radius:5px;border:1px solid #fff;}
            .center{text-align:center;margin:20px 0;}

            .btn{
            display:inline-block;
            padding:10px 20px;
            margin:5px;
            background:#555;
            color:#fff;
            text-decoration:none;
            border-radius:8px;
            cursor:pointer;
            }

            .btn:hover{background:#ff6600;}
            </style>
            </head>

            <body>

            <header>Ovládání barevné LED</header>

            <div class="slider-container">
            <label for="r">Červená: <span id="rval">0</span></label>
            <input type="range" id="r" min="0" max="255" value="0">
            </div>

            <div class="slider-container">
            <label for="g">Zelená: <span id="gval">0</span></label>
            <input type="range" id="g" min="0" max="255" value="0">
            </div>

            <div class="slider-container">
            <label for="b">Modrá: <span id="bval">0</span></label>
            <input type="range" id="b" min="0" max="255" value="0">
            </div>

            <div class="center">
            <div class="color-display" id="colorBox"></div>
            <div id="hexVal">HEX: #000000</div>

            <div>
            <a class="btn" id="resetBtn">Restart</a>
            <a class="btn" id="backBtn">Zpět</a>
            </div>
            </div>

            <script>

            function sendRGB(r,g,b){
            fetch("/rgb/update?r="+r+"&g="+g+"&b="+b);
            }

            function updateColorDisplay(send=true){

            let r=document.getElementById("r").value;
            let g=document.getElementById("g").value;
            let b=document.getElementById("b").value;

            document.getElementById("rval").textContent=r;
            document.getElementById("gval").textContent=g;
            document.getElementById("bval").textContent=b;

            document.getElementById("colorBox").style.backgroundColor="rgb("+r+","+g+","+b+")";

            document.getElementById("hexVal").textContent=
            "HEX: #"+(+r).toString(16).padStart(2,"0")+(+g).toString(16).padStart(2,"0")+(+b).toString(16).padStart(2,"0");

            if(send){
            sendRGB(r,g,b);
            }
            }

            window.onload=function(){

            document.getElementById("r").addEventListener("input",function(){updateColorDisplay(true);});
            document.getElementById("g").addEventListener("input",function(){updateColorDisplay(true);});
            document.getElementById("b").addEventListener("input",function(){updateColorDisplay(true);});

            document.getElementById("resetBtn").addEventListener("click",function(){
            fetch("/rgb/reset");
            });

            document.getElementById("backBtn").addEventListener("click",function(){
            fetch("/rgb/exit");
            window.location.href="/";
            });

            updateColorDisplay(false);

            };

            </script>

            </body>
            </html>
        )rawliteral";

    // Snake
        const char snakePage[] PROGMEM = R"rawliteral(
            <!DOCTYPE html>
            <html lang="cs">
            <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>Snake Leaderboard</title>

            <style>
            body{
                font-family: Arial, sans-serif;
                background:#222;
                color:#fff;
                margin:0;
                padding:0;
            }

            header{
                background:#444;
                padding:15px;
                text-align:center;
                font-size:1.5em;
            }

            nav{
                display:flex;
                flex-direction:column;
                margin:10px;
            }

            button{
                padding:15px;
                margin:5px 0;
                background:#555;
                border:none;
                border-radius:8px;
                color:white;
                font-size:1.1em;
                cursor:pointer;
            }

            button:hover{
                background:#ff6600;
            }

            table{
                width:100%;
                border-collapse:collapse;
                margin-top:10px;
            }

            th,td{
                padding:10px;
                text-align:center;
                border-bottom:1px solid #444;
            }

            input{
                padding:10px;
                margin-top:10px;
                width:100%;
                box-sizing:border-box;
                border-radius:6px;
                border:none;
            }

            footer{
                text-align:center;
                padding:10px;
                font-size:0.9em;
                color:#aaa;
            }
            </style>
            </head>

            <body>

            <header>Snake Leaderboard</header>

            <nav>

            <table id="leaderboard">
            <tr>
            <th>Jméno</th>
            <th>Obtížnost</th>
            <th>Max ovoce</th>
            </tr>
            </table>

            <input id="playerName" placeholder="Zadej jméno pro uložení výsledku" maxlength="20">
            <button onclick="saveName()">Uložit jméno</button>

            <button onclick="setDifficulty(0)">Lehká</button>
            <button onclick="setDifficulty(1)">Střední</button>
            <button onclick="setDifficulty(2)">Těžká</button>
            <button onclick="setDifficulty(3)">Chaos</button>
            <button onclick="goBack()">Zpět</button>

            </nav>

            <footer>Demonstrační firmware pro ESP32</footer>

            <script>
            function setDifficulty(level){
                fetch('/snake/setDifficulty?level='+level);
            }

            // ulozeni jmena
            function saveName(){
                let name = document.getElementById("playerName").value;
                if(name.length > 0){
                    fetch('/snake/setName?name='+encodeURIComponent(name));
                    document.getElementById("playerName").value = '';
                }
            }

            // tlačítko zpět
            function goBack(){
                fetch('/snake/exit')
                .then(() => {
                    window.location.href = '/snake';
                });
            }

            // dynamicke nacitani leaderboardu
            function updateLeaderboard(){
                fetch('/snake/leaderboard')
                .then(resp => resp.json())
                .then(data => {
                    data.sort((a,b)=>b.score-a.score); // seřadit sestupně podle max ovoce
                    let tbody = '';
                    for(let i=0;i<data.length;i++){
                        tbody += `<tr>
                            <td>${data[i].name}</td>
                            <td>${data[i].difficulty}</td>
                            <td>${data[i].score}</td>
                        </tr>`;
                    }
                    document.getElementById('leaderboard').innerHTML = `
                    <tr><th>Jméno</th><th>Obtížnost</th><th>Max ovoce</th></tr>` + tbody;
                });
            }

            // aktualizace kazde 2s
            setInterval(updateLeaderboard, 2000);
            updateLeaderboard();
            </script>

            </body>
            </html>
        )rawliteral";

    const char levelPage[] PROGMEM = R"rawliteral(
        <!DOCTYPE html>
        <html lang="cs">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>Level Page - ESP32 Demo</title>
            <style>
            body {
                font-family: Arial, sans-serif;
                background-color: #222;
                color: #fff;
                margin: 0;
                padding: 0;
            }
            header {
                background-color: #444;
                padding: 15px;
                text-align: center;
                font-size: 1.5em;
            }
            nav {
                display: flex;
                flex-direction: column;
                padding: 0;
                margin: 0;
                list-style: none;
                align-items: center;
                margin-top: 50px;
            }
            nav a {
                display: block;
                padding: 15px 30px;
                margin: 5px 0;
                background-color: #555;
                color: #fff;
                text-decoration: none;
                border-radius: 8px;
                text-align: center;
                font-size: 1.2em;
                transition: background 0.3s;
            }
            nav a:hover {
                background-color: #ff6600;
            }
            footer {
                text-align: center;
                padding: 10px;
                font-size: 0.9em;
                color: #aaa;
                position: fixed;
                width: 100%;
                bottom: 0;
            }
            </style>
        </head>
        <body>
            <header>Level Page</header>
            <nav>
                <a id="backBtn" href="/">Zpět</a>
            </nav>
            <footer>Demonstrační firmware pro ESP32</footer>
        <script>
            document.getElementById("backBtn").addEventListener("click",function(){
            fetch("/level/exit");
            window.location.href="/";
            });
        </script>
        </body>
        </html>
    )rawliteral";

    const char ppgPage[] PROGMEM = R"rawliteral(
        <!DOCTYPE html>
        <html lang="cs">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>PPG - ESP32 Demo</title>
            <style>
            body {
                font-family: Arial, sans-serif;
                background-color: #222;
                color: #fff;
                margin: 0;
                padding: 0;
            }
            header {
                background-color: #444;
                padding: 15px;
                text-align: center;
                font-size: 1.5em;
            }
            .container {
                text-align: center;
                margin-top: 40px;
            }
            .btn {
                display: inline-block;
                padding: 12px 24px;
                margin: 8px;
                background-color: #555;
                color: #fff;
                text-decoration: none;
                border-radius: 8px;
                cursor: pointer;
                border: none;
                font-size: 1em;
            }
            .btn:hover {
                background-color: #ff6600;
            }
            .muted {
                color: #aaa;
                margin-top: 12px;
                font-size: 0.9em;
            }
            </style>
        </head>
        <body>
            <header>PPG</header>
            <div class="container">
                <p>PPG běží na zařízení. Návrat do menu provedete tlačítkem níže.</p>
                <button class="btn" id="backBtn">Zpět</button>
                <div class="muted">Tip: aktuální hodnoty sleduj na OLED displeji zařízení.</div>
            </div>
            <script>
                document.getElementById("backBtn").addEventListener("click", function(){
                    fetch("/ppg/exit").then(() => {
                        window.location.href = "/";
                    });
                });
            </script>
        </body>
        </html>
    )rawliteral";
