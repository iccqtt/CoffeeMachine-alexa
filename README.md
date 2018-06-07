# CoffeeMachine-alexa
<img src="https://cdn.instructables.com/FFV/P2D0/J20P2331/FFVP2D0J20P2331.LARGE.jpg?auto=webp" width="480" height="300" />

This repository contains code necessary to run the Alexa powered Coffee Machine.
Each folder contains the respective code to be used (CSR source code is in CSR folder, and so on).
The final application consists of a voice controlled, alexa powered coffee machine with a companion android application.

# The instructables tutorial
The code here is an expansion on the Coffee Machine project, which [you can find more detailed instructions here.](https://www.instructables.com/id/How-to-Connect-a-Coffee-Machine-With-an-Android-Ap/) There are also more links on this page regarding other project components, such as CSR and Android application info.
# How to run this?
### Accounts
 * Create an [Amazon Developer](https://developer.amazon.com) account. (PS: you will also need an Amazon Web Services account later.)
 * Set up a [new Alexa device](https://developer.amazon.com/avs/home.html#/avs/home). Make sure to keep the product information readily available for future reference.

### Cognito
 * On Amazon Web Services, access the Cognito service, go to ```Manage federated entities```, then click on ```Create new identity pool```, give a name and check ```Enable access to unauthenticated identities```.
 * Get your cognito pool ID, clone this repository, and paste the ID code in ```AndroidApp/Cafeteira/app/src/main/java/cafeteira/com/cafeteira/controller/AWSConnection.java``` on constant ```COGNITO_POOL_ID```
 * Go to AWS IAM service, go to ```Policies``` and ```Create your own Policy```
 * Use the following policy JSON:
 ```JSON
 {
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "iot:AttachPrincipalPolicy",
                "iot:CreateKeysAndCertificate"
            ],
            "Resource": [
                "*"
            ]
        }
    ]
}
```
 * Go to ```Roles``` and select the *Unauth* role corresponding to the Cognito pool name you created before, go to ```Permissions``` > ```Attach Policy``` and attach your newly created policy to the role.

### AWS IoT

 * Now go to AWS IoT core, click on ```Secure``` and then ```Policies```.
 * Click on the ```Create``` button. Input a name, then click ```Advanced mode``` and paste the following code:
 ```JSON
 {
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish",
        "iot:Subscribe",
        "iot:Receive"
      ],    
      "Resource": "*"
    }
  ]
}
 ```
 * Now, go to ```Manage``` on the sidebar and select ```Create```.
 * Give the device a name, click next, and choose ```One-Click certificate creation```.
 * Download *ALL* certificates in this page, including the **root-CA.crt**. Place these on ```/Dragonboard410c/CoffeeMachine/authenticated```. Then, click *activate*, and go to ```Attach a policy```.
 * Select your newly created policy from the list, and click ```Done```.
 * Now, click on your newly created device, go to ```Interact```.
 * Copy the HTTPS url to the constant CUSTOMER_SPECIFIC_ENDPOINT in ```AWSConnection.java``` in the Android App.
 * In ```AWSConnection.java```, the constant AWS_IOT_POLICY_NAME *must be set* to match the policy name you created earlier for AWS IoT.
 * In ```Dragonboard410c/CoffeeMachine/ClientAWS.py```, you'll have to set the path to the certificates you downloaded, the "host" variable to the *Thing* HTTPS url and "clientID" to the *Thing* ID name.

### Alexa skill
 * In the Amazon Developer account you used earlier, create your Alexa skill in the Alexa Skills Kit.
 * You'll need an invocation name, a TurnCoffeeMachine and MakeCoffee intents.
 * Suggested TurnCoffeeMachine phrases:
 ```
 TurnCoffeeMachine turn {CoffeeState}
 TurnCoffeeMachine {CoffeeState} coffee machine
 TurnCoffeeMachine {CoffeeState} the coffee machine
 TurnCoffeeMachine turn {CoffeeState} the coffee machine
 ```
 * Suggested MakeCoffee phrases:
 ```
 MakeCoffee {CoffeeType} coffee
 MakeCoffee make {CoffeeType} coffee
 ```
 * You'll need to create two slot types:
 * -> COFFEE_STATE with two values: 'on' and 'off'.
 * -> COFFEE_TYPE with two values: 'short' and 'long'.

### AWS Lambda
 * Create a new AWS Lambda function (make sure the region matches!) using the code provided in the ```AWS_Lambda``` folder.
 * You will need to create an API to control environment variables on Lambda. (More on this below)
 * Environment variables used by Lambda (must be created):
 ```
 coffelevel         //Stores how much coffee is readily available in the machine
 glassposition      //Stores last cup sensor state
 on_off             //Stores current machine status
 waterlevel         //Stores how much water is on the water reservoir
 ```
#### API Gateway
>In order to manipulate environment variables on Lambda, an API must be created.
 * Go to AWS API Gateway and click ```Create API```
 * Give it a name, a description, and leave ```Regional``` checked.
 * Select ```Actions``` and create a new resource named **status** with path ```/status```
 * Then, setup the *POST* and *GET* methods. Make sure you link them to your Lambda function!
 * The API url **must** be set on ```lambda_function.py``` (line 332) and ```ServiceHTTP.py```(line 6)!

### CSR1010 code
>This code runs on the embedded bluetooth module.

* Using a CSR device connected to a Windows machine, install the CSR SDK and run the IDE.
* Open the CSR project code and download it to the CSR module.

>[This instructable](https://www.instructables.com/id/How-to-Connect-a-Coffee-Machine-With-an-Android-Ap/) has more information on the CSR sensor and how to control it.

### AlexaPi + IoT controller
> This code runs on the embedded board! We recommend using a [Dragonboard 410C for this.](https://developer.qualcomm.com/hardware/dragonboard-410c)

 * On Amazon Developer, go to the Alexa Voice Service, select your device and go to ```Manage```.
 * Click on ```Security Profile``` and add ```http://localhost:5050``` and ```https://localhost:5050```to the *Allowed Origins*.
 * Add ```http://localhost:5050/code``` and ```https://localhost:5050/code``` to *Allowed return URLS*.
 * Clone the repository to the embedded device, navigate to the ```Dragonboard410c/CoffeeMachine/scripts```, make ```setup.sh``` executable and run as super user.
 * When prompted, refuse to stop the alexapi install. You might not want to use Airplay.
 * You will then be prompted to input device information to generate authentication keys. This information is found on AVS itself.
 * Follow on-screen instructions to obtain the Auth key.
 * Lastly, install ```AWSIoTPythonSDK``` package on pip by running:
 ```
 sudo pip install AWSIoTPythonSDK
 ```
 * Make sure you have a headset connected to the embedded device, and set it as primary communications device. Do **not** start *alexapi* through SSH, or it will fail!
 * Run **main.py**. Once CSR connection has succeeded, alexa begins listening to the wakeword.
 * Use either your voice or the Android app to control and operate the coffee machine.

#### Suggested phrases:
 * "Alexa, ask coffee machine turn on"
 * "Alexa, ask coffee machine make long coffee"
 * "Alexa, ask coffee machine make short coffee"
 * "Alexa, ask coffee machine turn off"

>PS: The coffee machine won't make coffee if water isn't hot enough. Give it some time to heat up the water and try again.

-----
### Contact
[Send us an e-mail here.](mailto:qtticcteste@gmail.com)
