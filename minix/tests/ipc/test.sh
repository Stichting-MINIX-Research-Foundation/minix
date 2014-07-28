for d in semctl semget semop shmat shmctl shmdt shmget shmt
do	echo $d
	(	cd $d 
		sh test.sh
	)
done

