let array= ["bmw", "sonata", "flat"], car = {
    name : "sonata",
    hp : 1000,
    start : function (){
        console.log("engine stop");
    },
    stop : function () {
        console.log("engine stop");
    } 
};      // 배열에 object가 들어갈 수도 있음 

console.log(array[0]);
console.log(array[1]);
console.log(array[2]);
console.log(array[3]);  // undefined로 출력 


