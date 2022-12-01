/* sessionid generates a unique session id */
package main

import (
	"fmt"
	"time"

	"github.com/google/uuid"
)

func main() {
	fmt.Printf("%s_%s\n",
		time.Now().UTC().Format("20060102T150405Z"),
		uuid.NewString())

}
